#ifndef SkTypeface_cairo_DEFINED
#define SkTypeface_cairo_DEFINED

#include <cairo.h>
#include <cairo-ft.h>

#include "SkTypeface.h"
#include "SkSurfaceProps.h"

SK_API extern void SkInitCairoFT(bool fontHintingEnabled);

SK_API extern SkTypeface* SkCreateTypefaceFromCairoFTFont(cairo_scaled_font_t* scaledFont, FT_Face face = nullptr, void* faceContext = nullptr, SkPixelGeometry pixelGeometry = kUnknown_SkPixelGeometry, uint8_t lcdFilter = 0);

#endif

