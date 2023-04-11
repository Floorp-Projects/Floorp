#ifndef SkTypeface_cairo_DEFINED
#define SkTypeface_cairo_DEFINED

#include "include/core/SkTypeface.h"
#include "include/core/SkSurfaceProps.h"

struct FT_FaceRec_;
typedef FT_FaceRec_* FT_Face;

SK_API extern void SkInitCairoFT(bool fontHintingEnabled);

SK_API extern SkTypeface* SkCreateTypefaceFromCairoFTFont(
    FT_Face face = nullptr, void* faceContext = nullptr,
    SkPixelGeometry pixelGeometry = kUnknown_SkPixelGeometry,
    uint8_t lcdFilter = 0);

#endif

