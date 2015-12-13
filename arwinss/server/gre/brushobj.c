/* 
 * COPYRIGHT:         See COPYING in the top level directory
 * PROJECT:           ReactOS win32 subsystem
 * PURPOSE:           Functions for brushes
 * FILE:              subsystem/win32/win32k/objects/brush.c
 * PROGRAMER:         
 */

#include <win32k.h>

#define NDEBUG
#include <debug.h>

static const USHORT HatchBrushes[NB_HATCH_STYLES][8] =
{
    {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF}, /* HS_HORIZONTAL */
    {0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7}, /* HS_VERTICAL   */
    {0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F}, /* HS_FDIAGONAL  */
    {0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFE}, /* HS_BDIAGONAL  */
    {0xF7, 0xF7, 0xF7, 0xF7, 0x00, 0xF7, 0xF7, 0xF7}, /* HS_CROSS      */
    {0x7E, 0xBD, 0xDB, 0xE7, 0xE7, 0xDB, 0xBD, 0x7E}  /* HS_DIAGCROSS  */
};

BOOL
INTERNAL_CALL
BRUSH_Cleanup(PVOID ObjectBody)
{
    PBRUSH pbrush = (PBRUSH)ObjectBody;
    if (pbrush->flAttrs & (GDIBRUSH_IS_HATCH | GDIBRUSH_IS_BITMAP))
    {
        ASSERT(pbrush->hbmPattern);
        GDIOBJ_SetOwnership(pbrush->hbmPattern, PsGetCurrentProcess());
        GreDeleteObject(pbrush->hbmPattern);
    }

    /* Free the kmode styles array of EXTPENS */
    if (pbrush->pStyle)
    {
        ExFreePool(pbrush->pStyle);
    }

    return TRUE;
}

INT
FASTCALL
BRUSH_GetObject(PBRUSH pbrush, INT Count, LPLOGBRUSH Buffer)
{
    if (Buffer == NULL) return sizeof(LOGBRUSH);
    if (Count == 0) return 0;

    /* Set colour */
    Buffer->lbColor = pbrush->BrushAttr.lbColor;

    /* Set Hatch */
    if ((pbrush->flAttrs & GDIBRUSH_IS_HATCH)!=0)
    {
        /* FIXME : this is not the right value */
        Buffer->lbHatch = (LONG)pbrush->hbmPattern;
    }
    else
    {
        Buffer->lbHatch = 0;
    }

    Buffer->lbStyle = 0;

    /* Get the type of style */
    if ((pbrush->flAttrs & GDIBRUSH_IS_SOLID)!=0)
    {
        Buffer->lbStyle = BS_SOLID;
    }
    else if ((pbrush->flAttrs & GDIBRUSH_IS_NULL)!=0)
    {
        Buffer->lbStyle = BS_NULL; // BS_HOLLOW
    }
    else if ((pbrush->flAttrs & GDIBRUSH_IS_HATCH)!=0)
    {
        Buffer->lbStyle = BS_HATCHED;
    }
    else if ((pbrush->flAttrs & GDIBRUSH_IS_BITMAP)!=0)
    {
        Buffer->lbStyle = BS_PATTERN;
    }
    else if ((pbrush->flAttrs & GDIBRUSH_IS_DIB)!=0)
    {
        Buffer->lbStyle = BS_DIBPATTERN;
    }

    /* FIXME
    else if ((pbrush->flAttrs & )!=0)
    {
        Buffer->lbStyle = BS_INDEXED;
    }
    else if ((pbrush->flAttrs & )!=0)
    {
        Buffer->lbStyle = BS_DIBPATTERNPT;
    }
    */

    /* FIXME */
    return sizeof(LOGBRUSH);
}

/**
 * @name CalculateColorTableSize
 *
 * Internal routine to calculate the number of color table entries.
 *
 * @param BitmapInfoHeader
 *        Input bitmap information header, can be any version of
 *        BITMAPINFOHEADER or BITMAPCOREHEADER.
 *
 * @param ColorSpec
 *        Pointer to variable which specifiing the color mode (DIB_RGB_COLORS
 *        or DIB_RGB_COLORS). On successful return this value is normalized
 *        according to the bitmap info.
 *
 * @param ColorTableSize
 *        On successful return this variable is filled with number of
 *        entries in color table for the image with specified parameters.
 *
 * @return
 *    TRUE if the input values together form a valid image, FALSE otherwise.
 */
BOOL
APIENTRY
CalculateColorTableSize(
    CONST BITMAPINFOHEADER *BitmapInfoHeader,
    UINT *ColorSpec,
    UINT *ColorTableSize)
{
    WORD BitCount;
    DWORD ClrUsed;
    DWORD Compression;

    /*
     * At first get some basic parameters from the passed BitmapInfoHeader
     * structure. It can have one of the following formats:
     * - BITMAPCOREHEADER (the oldest one with totally different layout
     *                     from the others)
     * - BITMAPINFOHEADER (the standard and most common header)
     * - BITMAPV4HEADER (extension of BITMAPINFOHEADER)
     * - BITMAPV5HEADER (extension of BITMAPV4HEADER)
     */
    if (BitmapInfoHeader->biSize == sizeof(BITMAPCOREHEADER))
    {
        BitCount = ((LPBITMAPCOREHEADER)BitmapInfoHeader)->bcBitCount;
        ClrUsed = 0;
        Compression = BI_RGB;
    }
    else
    {
        BitCount = BitmapInfoHeader->biBitCount;
        ClrUsed = BitmapInfoHeader->biClrUsed;
        Compression = BitmapInfoHeader->biCompression;
    }

    switch (Compression)
    {
        case BI_BITFIELDS:
            if (*ColorSpec == DIB_PAL_COLORS)
                *ColorSpec = DIB_RGB_COLORS;

            if (BitCount != 16 && BitCount != 32)
                return FALSE;

            /* For BITMAPV4HEADER/BITMAPV5HEADER the masks are included in
             * the structure itself (bV4RedMask, bV4GreenMask, and bV4BlueMask).
             * For BITMAPINFOHEADER the color masks are stored in the palette. */
            if (BitmapInfoHeader->biSize > sizeof(BITMAPINFOHEADER))
                *ColorTableSize = 0;
            else
                *ColorTableSize = 3;

            return TRUE;

        case BI_RGB:
            switch (BitCount)
            {
                case 1:
                    *ColorTableSize = ClrUsed ? min(ClrUsed, 2) : 2;
                    return TRUE;

                case 4:
                    *ColorTableSize = ClrUsed ? min(ClrUsed, 16) : 16;
                    return TRUE;

                case 8:
                    *ColorTableSize = ClrUsed ? min(ClrUsed, 256) : 256;
                    return TRUE;

                default:
                    if (*ColorSpec == DIB_PAL_COLORS)
                        *ColorSpec = DIB_RGB_COLORS;
                    if (BitCount != 16 && BitCount != 24 && BitCount != 32)
                        return FALSE;
                    *ColorTableSize = ClrUsed;
                    return TRUE;
            }

        case BI_RLE4:
            if (BitCount == 4)
            {
                *ColorTableSize = ClrUsed ? min(ClrUsed, 16) : 16;
                return TRUE;
            }
            return FALSE;

        case BI_RLE8:
            if (BitCount == 8)
            {
                *ColorTableSize = ClrUsed ? min(ClrUsed, 256) : 256;
                return TRUE;
            }
            return FALSE;

        case BI_JPEG:
        case BI_PNG:
            *ColorTableSize = ClrUsed;
            return TRUE;

        default:
            return FALSE;
    }
}

HBRUSH
APIENTRY
GreCreateDIBBrush(
    CONST BITMAPINFO *BitmapInfo,
    UINT ColorSpec,
    UINT BitmapInfoSize,
    CONST VOID *PackedDIB)
{
    HBRUSH hBrush;
    PBRUSH pbrush;
    HBITMAP hPattern;
    ULONG_PTR DataPtr;
    UINT PaletteEntryCount;
    PSURFACE psurfPattern;
    INT PaletteType;

    if (BitmapInfo->bmiHeader.biSize < sizeof(BITMAPINFOHEADER))
    {
        SetLastWin32Error(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (!CalculateColorTableSize(&BitmapInfo->bmiHeader,
                                 &ColorSpec,
                                 &PaletteEntryCount))
    {
        SetLastWin32Error(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    // FIXME: What about BI_BITFIELDS
    DataPtr = (ULONG_PTR)BitmapInfo + BitmapInfo->bmiHeader.biSize;
    if (ColorSpec == DIB_RGB_COLORS)
        DataPtr += PaletteEntryCount * sizeof(RGBQUAD);
    else
        DataPtr += PaletteEntryCount * sizeof(USHORT);

    hPattern = IntGdiCreateBitmap(BitmapInfo->bmiHeader.biWidth,
                                  BitmapInfo->bmiHeader.biHeight,
                                  BitmapInfo->bmiHeader.biPlanes,
                                  BitmapInfo->bmiHeader.biBitCount,
                                  (PVOID)DataPtr);
    if (hPattern == NULL)
    {
        SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    psurfPattern = SURFACE_LockSurface(hPattern);
    ASSERT(psurfPattern != NULL);
    psurfPattern->hDIBPalette = BuildDIBPalette(BitmapInfo, &PaletteType);
    SURFACE_UnlockSurface(psurfPattern);

    pbrush = BRUSH_AllocBrushWithHandle();
    if (pbrush == NULL)
    {
        GreDeleteObject(hPattern);
        SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    hBrush = pbrush->BaseObject.hHmgr;

    pbrush->flAttrs |= GDIBRUSH_IS_BITMAP | GDIBRUSH_IS_DIB;
    pbrush->hbmPattern = hPattern;
    /* FIXME: Fill in the rest of fields!!! */

    GDIOBJ_SetOwnership(hPattern, NULL);

    BRUSH_UnlockBrush(pbrush);

    return hBrush;
}

HBRUSH
APIENTRY
GreCreateHatchBrush(
    INT Style,
    COLORREF Color)
{
    HBRUSH hBrush;
    PBRUSH pbrush;
    HBITMAP hPattern;

    if (Style < 0 || Style >= NB_HATCH_STYLES)
    {
        return 0;
    }

    hPattern = IntGdiCreateBitmap(8, 8, 1, 1, (LPBYTE)HatchBrushes[Style]);
    if (hPattern == NULL)
    {
        SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    pbrush = BRUSH_AllocBrushWithHandle();
    if (pbrush == NULL)
    {
        GreDeleteObject(hPattern);
        SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    hBrush = pbrush->BaseObject.hHmgr;

    pbrush->flAttrs |= GDIBRUSH_IS_HATCH;
    pbrush->hbmPattern = hPattern;
    pbrush->BrushAttr.lbColor = Color & 0xFFFFFF;

    GDIOBJ_SetOwnership(hPattern, NULL);

    BRUSH_UnlockBrush(pbrush);

    return hBrush;
}

HBRUSH
APIENTRY
GreCreatePatternBrush(
    HBITMAP hBitmap)
{
    HBRUSH hBrush;
    PBRUSH pbrush;
    HBITMAP hPattern;

    hPattern = BITMAP_CopyBitmap(hBitmap);
    if (hPattern == NULL)
    {
        SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    pbrush = BRUSH_AllocBrushWithHandle();
    if (pbrush == NULL)
    {
        GreDeleteObject(hPattern);
        SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    hBrush = pbrush->BaseObject.hHmgr;

    pbrush->flAttrs |= GDIBRUSH_IS_BITMAP;
    pbrush->hbmPattern = hPattern;
    /* FIXME: Fill in the rest of fields!!! */

    GDIOBJ_SetOwnership(hPattern, NULL);

    BRUSH_UnlockBrush(pbrush);

    return hBrush;
}

HBRUSH
APIENTRY
GreCreateSolidBrush(
    COLORREF Color)
{
    HBRUSH hBrush;
    PBRUSH pbrush;

    pbrush = BRUSH_AllocBrushWithHandle();
    if (pbrush == NULL)
    {
        SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    hBrush = pbrush->BaseObject.hHmgr;

    pbrush->flAttrs |= GDIBRUSH_IS_SOLID;

    pbrush->BrushAttr.lbColor = Color;
    /* FIXME: Fill in the rest of fields!!! */

    BRUSH_UnlockBrush(pbrush);

    return hBrush;
}

HBRUSH
APIENTRY
GreCreateNullBrush(VOID)
{
    HBRUSH hBrush;
    PBRUSH pbrush;

    pbrush = BRUSH_AllocBrushWithHandle();
    if (pbrush == NULL)
    {
        SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    hBrush = pbrush->BaseObject.hHmgr;

    pbrush->flAttrs |= GDIBRUSH_IS_NULL;
    BRUSH_UnlockBrush(pbrush);

    return hBrush;
}

VOID
FASTCALL
IntGdiSetSolidBrushColor(HBRUSH hBrush, COLORREF Color)
{
    PBRUSH pbrush;

    pbrush = BRUSH_LockBrush(hBrush);
    if (pbrush->flAttrs & GDIBRUSH_IS_SOLID)
    {
        pbrush->BrushAttr.lbColor = Color & 0xFFFFFF;
    }
    BRUSH_UnlockBrush(pbrush);
}

/* EOF */