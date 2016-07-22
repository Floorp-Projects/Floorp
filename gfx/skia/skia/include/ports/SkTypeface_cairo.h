#ifndef SkTypeface_cairo_DEFINED
#define SkTypeface_cairo_DEFINED

#include <cairo.h>
#include <cairo-ft.h>

#include "SkTypeface.h"

SK_API extern void SkInitCairoFT(bool fontHintingEnabled);

SK_API extern SkTypeface* SkCreateTypefaceFromCairoFTFont(cairo_scaled_font_t* scaledFont);

#ifdef CAIRO_HAS_FC_FONT
SK_API extern SkTypeface* SkCreateTypefaceFromCairoFTFontWithFontconfig(cairo_scaled_font_t* scaledFont, FcPattern* pattern);
#endif

#endif

