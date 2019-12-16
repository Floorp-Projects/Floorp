
/*
 * Copyright 2012 Mozilla Foundation
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/ports/SkFontHost_FreeType_common.h"

#include "src/core/SkAdvancedTypefaceMetrics.h"
#include "src/core/SkFDot6.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkPath.h"
#include "src/core/SkScalerContext.h"
#include "src/core/SkTypefaceCache.h"

#include <cmath>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

// for FT_GlyphSlot_Embolden
#ifdef FT_SYNTHESIS_H
#include FT_SYNTHESIS_H
#endif

// for FT_Library_SetLcdFilter
#ifdef FT_LCD_FILTER_H
#include FT_LCD_FILTER_H
#else
typedef enum FT_LcdFilter_
{
    FT_LCD_FILTER_NONE    = 0,
    FT_LCD_FILTER_DEFAULT = 1,
    FT_LCD_FILTER_LIGHT   = 2,
    FT_LCD_FILTER_LEGACY  = 16,
} FT_LcdFilter;
#endif

// If compiling with FreeType before 2.5.0
#ifndef FT_LOAD_COLOR
#    define FT_LOAD_COLOR ( 1L << 20 )
#    define FT_PIXEL_MODE_BGRA 7
#endif

#ifndef SK_CAN_USE_DLOPEN
#define SK_CAN_USE_DLOPEN 1
#endif
#if SK_CAN_USE_DLOPEN
#include <dlfcn.h>
#endif

#ifndef SK_FONTHOST_CAIRO_STANDALONE
#define SK_FONTHOST_CAIRO_STANDALONE 1
#endif

static bool gFontHintingEnabled = true;
static FT_Error (*gSetLcdFilter)(FT_Library, FT_LcdFilter) = nullptr;
static void (*gGlyphSlotEmbolden)(FT_GlyphSlot) = nullptr;

extern "C"
{
    void mozilla_LockFTLibrary(FT_Library aLibrary);
    void mozilla_UnlockFTLibrary(FT_Library aLibrary);
    void mozilla_AddRefSharedFTFace(void* aContext);
    void mozilla_ReleaseSharedFTFace(void* aContext, void* aOwner);
    void mozilla_ForgetSharedFTFaceLockOwner(void* aContext, void* aOwner);
    int mozilla_LockSharedFTFace(void* aContext, void* aOwner);
    void mozilla_UnlockSharedFTFace(void* aContext);
    FT_Error mozilla_LoadFTGlyph(FT_Face aFace, uint32_t aGlyphIndex, int32_t aFlags);
}

void SkInitCairoFT(bool fontHintingEnabled)
{
    gFontHintingEnabled = fontHintingEnabled;
#if SK_CAN_USE_DLOPEN
    gSetLcdFilter = (FT_Error (*)(FT_Library, FT_LcdFilter))dlsym(RTLD_DEFAULT, "FT_Library_SetLcdFilter");
    gGlyphSlotEmbolden = (void (*)(FT_GlyphSlot))dlsym(RTLD_DEFAULT, "FT_GlyphSlot_Embolden");
#else
    gSetLcdFilter = &FT_Library_SetLcdFilter;
    gGlyphSlotEmbolden = &FT_GlyphSlot_Embolden;
#endif
    // FT_Library_SetLcdFilter may be provided but have no effect if FreeType
    // is built without FT_CONFIG_OPTION_SUBPIXEL_RENDERING.
    if (gSetLcdFilter &&
        gSetLcdFilter(nullptr, FT_LCD_FILTER_NONE) == FT_Err_Unimplemented_Feature) {
        gSetLcdFilter = nullptr;
    }
}

class SkScalerContext_CairoFT : public SkScalerContext_FreeType_Base {
public:
    SkScalerContext_CairoFT(sk_sp<SkTypeface> typeface,
                            const SkScalerContextEffects& effects,
                            const SkDescriptor* desc, FT_Face face,
                            void* faceContext, SkPixelGeometry pixelGeometry,
                            FT_LcdFilter lcdFilter);

    virtual ~SkScalerContext_CairoFT() {
        mozilla_ForgetSharedFTFaceLockOwner(fFTFaceContext, this);
    }

    bool isValid() const { return fFTFaceContext != nullptr; }

    void Lock() {
        if (!mozilla_LockSharedFTFace(fFTFaceContext, this)) {
            FT_Set_Transform(fFTFace, fHaveShape ? &fShapeMatrixFT : nullptr, nullptr);
            FT_Set_Char_Size(fFTFace, FT_F26Dot6(fScaleX * 64.0f + 0.5f),
                             FT_F26Dot6(fScaleY * 64.0f + 0.5f), 0, 0);
        }
    }

    void Unlock() { mozilla_UnlockSharedFTFace(fFTFaceContext); }

protected:
    unsigned generateGlyphCount() override;
    bool generateAdvance(SkGlyph* glyph) override;
    void generateMetrics(SkGlyph* glyph) override;
    void generateImage(const SkGlyph& glyph) override;
    bool generatePath(SkGlyphID glyphID, SkPath* path) override;
    void generateFontMetrics(SkFontMetrics* metrics) override;

private:
    bool computeShapeMatrix(const SkMatrix& m);
    void prepareGlyph(FT_GlyphSlot glyph);

    FT_Face fFTFace;
    void* fFTFaceContext;
    FT_Int32 fLoadGlyphFlags;
    FT_LcdFilter fLcdFilter;
    SkScalar fScaleX;
    SkScalar fScaleY;
    SkMatrix fShapeMatrix;
    FT_Matrix fShapeMatrixFT;
    bool fHaveShape;
};

class AutoLockFTFace {
public:
    AutoLockFTFace(SkScalerContext_CairoFT* scalerContext)
        : fScalerContext(scalerContext) {
        fScalerContext->Lock();
    }

    ~AutoLockFTFace() { fScalerContext->Unlock(); }

private:
    SkScalerContext_CairoFT* fScalerContext;
};

static bool isLCD(const SkScalerContextRec& rec) {
    return SkMask::kLCD16_Format == rec.fMaskFormat;
}

static bool bothZero(SkScalar a, SkScalar b) {
    return 0 == a && 0 == b;
}

// returns false if there is any non-90-rotation or skew
static bool isAxisAligned(const SkScalerContextRec& rec) {
    return 0 == rec.fPreSkewX &&
           (bothZero(rec.fPost2x2[0][1], rec.fPost2x2[1][0]) ||
            bothZero(rec.fPost2x2[0][0], rec.fPost2x2[1][1]));
}

class SkCairoFTTypeface : public SkTypeface {
public:
    std::unique_ptr<SkStreamAsset> onOpenStream(int*) const override { return nullptr; }

    std::unique_ptr<SkAdvancedTypefaceMetrics> onGetAdvancedMetrics() const override
    {
        SkDEBUGCODE(SkDebugf("SkCairoFTTypeface::onGetAdvancedMetrics unimplemented\n"));
        return nullptr;
    }

    SkScalerContext* onCreateScalerContext(const SkScalerContextEffects& effects, const SkDescriptor* desc) const override
    {
        SkScalerContext_CairoFT* ctx = new SkScalerContext_CairoFT(
            sk_ref_sp(const_cast<SkCairoFTTypeface*>(this)), effects, desc,
            fFTFace, fFTFaceContext, fPixelGeometry, fLcdFilter);
        if (!ctx->isValid()) {
            delete ctx;
            return nullptr;
        }
        return ctx;
    }

    void onFilterRec(SkScalerContextRec* rec) const override
    {
        // rotated text looks bad with hinting, so we disable it as needed
        if (!gFontHintingEnabled || !isAxisAligned(*rec)) {
            rec->setHinting(SkFontHinting::kNone);
        }

        // Don't apply any gamma so that we match cairo-ft's results.
        rec->ignorePreBlend();
    }

    void onGetFontDescriptor(SkFontDescriptor*, bool*) const override
    {
        SkDEBUGCODE(SkDebugf("SkCairoFTTypeface::onGetFontDescriptor unimplemented\n"));
    }

    void onCharsToGlyphs(const SkUnichar* chars, int count, SkGlyphID glyphs[]) const override
    {
        mozilla_LockSharedFTFace(fFTFaceContext, nullptr);
        for (int i = 0; i < count; ++i) {
            glyphs[i] = SkToU16(FT_Get_Char_Index(fFTFace, chars[i]));
        }
        mozilla_UnlockSharedFTFace(fFTFaceContext);
    }

    int onCountGlyphs() const override
    {
        return fFTFace->num_glyphs;
    }

    int onGetUPEM() const override
    {
        return 0;
    }

    SkTypeface::LocalizedStrings* onCreateFamilyNameIterator() const override
    {
        return nullptr;
    }

    void onGetFamilyName(SkString* familyName) const override
    {
        familyName->reset();
    }

    int onGetTableTags(SkFontTableTag*) const override
    {
        return 0;
    }

    size_t onGetTableData(SkFontTableTag, size_t, size_t, void*) const override
    {
        return 0;
    }

    void getPostScriptGlyphNames(SkString*) const override {}

    void getGlyphToUnicodeMap(SkUnichar*) const override {}

    int onGetVariationDesignPosition(SkFontArguments::VariationPosition::Coordinate coordinates[],
                                     int coordinateCount) const override
    {
        return 0;
    }

    int onGetVariationDesignParameters(SkFontParameters::Variation::Axis parameters[],
                                       int parameterCount) const override
    {
        return 0;
    }

    sk_sp<SkTypeface> onMakeClone(const SkFontArguments& args) const override {
        return sk_ref_sp(this);
    }

    SkCairoFTTypeface(FT_Face face, void* faceContext,
                      SkPixelGeometry pixelGeometry, FT_LcdFilter lcdFilter)
        : SkTypeface(SkFontStyle::Normal())
        , fFTFace(face)
        , fFTFaceContext(faceContext)
        , fPixelGeometry(pixelGeometry)
        , fLcdFilter(lcdFilter)
    {
        mozilla_AddRefSharedFTFace(fFTFaceContext);
    }

    void* GetFTFaceContext() const { return fFTFaceContext; }

    bool hasColorGlyphs() const override
    {
        // Check if the font has scalable outlines. If not, then avoid trying
        // to render it as a path.
        if (fFTFace) {
            return !FT_IS_SCALABLE(fFTFace);
        }
        return false;
    }

private:
    ~SkCairoFTTypeface()
    {
        mozilla_ReleaseSharedFTFace(fFTFaceContext, nullptr);
    }

    FT_Face            fFTFace;
    void*              fFTFaceContext;
    SkPixelGeometry    fPixelGeometry;
    FT_LcdFilter       fLcdFilter;
};

static bool FindByFTFaceContext(SkTypeface* typeface, void* context) {
    return static_cast<SkCairoFTTypeface*>(typeface)->GetFTFaceContext() == context;
}

SkTypeface* SkCreateTypefaceFromCairoFTFont(FT_Face face, void* faceContext,
                                            SkPixelGeometry pixelGeometry,
                                            uint8_t lcdFilter)
{
    sk_sp<SkTypeface> typeface =
        SkTypefaceCache::FindByProcAndRef(FindByFTFaceContext, faceContext);
    if (!typeface) {
        typeface = sk_make_sp<SkCairoFTTypeface>(face, faceContext, pixelGeometry,
                                                 (FT_LcdFilter)lcdFilter);
        SkTypefaceCache::Add(typeface);
    }

    return typeface.release();
}

SkScalerContext_CairoFT::SkScalerContext_CairoFT(
    sk_sp<SkTypeface> typeface, const SkScalerContextEffects& effects,
    const SkDescriptor* desc, FT_Face face, void* faceContext,
    SkPixelGeometry pixelGeometry, FT_LcdFilter lcdFilter)
    : SkScalerContext_FreeType_Base(std::move(typeface), effects, desc)
    , fFTFace(face)
    , fFTFaceContext(faceContext)
    , fLcdFilter(lcdFilter)
{
    SkMatrix matrix;
    fRec.getSingleMatrix(&matrix);

    computeShapeMatrix(matrix);

    FT_Int32 loadFlags = FT_LOAD_DEFAULT;

    if (SkMask::kBW_Format == fRec.fMaskFormat) {
        if (fRec.getHinting() == SkFontHinting::kNone) {
            loadFlags |= FT_LOAD_NO_HINTING;
        } else {
            loadFlags = FT_LOAD_TARGET_MONO;
        }
        loadFlags |= FT_LOAD_MONOCHROME;
    } else {
        if (isLCD(fRec)) {
            switch (pixelGeometry) {
            case kRGB_H_SkPixelGeometry:
            default:
                break;
            case kRGB_V_SkPixelGeometry:
                fRec.fFlags |= SkScalerContext::kLCD_Vertical_Flag;
                break;
            case kBGR_H_SkPixelGeometry:
                fRec.fFlags |= SkScalerContext::kLCD_BGROrder_Flag;
                break;
            case kBGR_V_SkPixelGeometry:
                fRec.fFlags |= SkScalerContext::kLCD_Vertical_Flag |
                               SkScalerContext::kLCD_BGROrder_Flag;
                break;
            }
        }

        switch (fRec.getHinting()) {
        case SkFontHinting::kNone:
            loadFlags |= FT_LOAD_NO_HINTING;
            break;
        case SkFontHinting::kSlight:
            loadFlags = FT_LOAD_TARGET_LIGHT;  // This implies FORCE_AUTOHINT
            break;
        case SkFontHinting::kNormal:
            if (fRec.fFlags & SkScalerContext::kForceAutohinting_Flag) {
                loadFlags |= FT_LOAD_FORCE_AUTOHINT;
            }
            break;
        case SkFontHinting::kFull:
            if (isLCD(fRec)) {
                if (fRec.fFlags & SkScalerContext::kLCD_Vertical_Flag) {
                    loadFlags = FT_LOAD_TARGET_LCD_V;
                } else {
                    loadFlags = FT_LOAD_TARGET_LCD;
                }
            }
            if (fRec.fFlags & SkScalerContext::kForceAutohinting_Flag) {
                loadFlags |= FT_LOAD_FORCE_AUTOHINT;
            }
            break;
        default:
            SkDebugf("---------- UNKNOWN hinting %d\n", fRec.getHinting());
            break;
        }
    }

    // Disable autohinting when asked to disable hinting, except for "tricky" fonts.
    if (!gFontHintingEnabled) {
        if (fFTFace && !(fFTFace->face_flags & FT_FACE_FLAG_TRICKY)) {
            loadFlags |= FT_LOAD_NO_AUTOHINT;
        }
    }

    if ((fRec.fFlags & SkScalerContext::kEmbeddedBitmapText_Flag) == 0) {
        loadFlags |= FT_LOAD_NO_BITMAP;
    }

    // Always using FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH to get correct
    // advances, as fontconfig and cairo do.
    // See http://code.google.com/p/skia/issues/detail?id=222.
    loadFlags |= FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;

    loadFlags |= FT_LOAD_COLOR;

    fLoadGlyphFlags = loadFlags;
}

bool SkScalerContext_CairoFT::computeShapeMatrix(const SkMatrix& m)
{
    // Compute a shape matrix compatible with Cairo's _compute_transform.
    // Finds major/minor scales and uses them to normalize the transform.
    double scaleX = m.getScaleX();
    double skewX = m.getSkewX();
    double skewY = m.getSkewY();
    double scaleY = m.getScaleY();
    double det = scaleX * scaleY - skewY * skewX;
    if (!std::isfinite(det)) {
        fScaleX = fRec.fTextSize * fRec.fPreScaleX;
        fScaleY = fRec.fTextSize;
        fHaveShape = false;
        return false;
    }
    double major = det != 0.0 ? hypot(scaleX, skewY) : 0.0;
    double minor = major != 0.0 ? fabs(det) / major : 0.0;
    // Limit scales to be above 1pt.
    major = SkTMax(major, 1.0);
    minor = SkTMax(minor, 1.0);

    // If the font is not scalable, then choose the best available size.
    if (fFTFace && !FT_IS_SCALABLE(fFTFace)) {
        double bestDist = DBL_MAX;
        FT_Int bestSize = -1;
        for (FT_Int i = 0; i < fFTFace->num_fixed_sizes; i++) {
            // Distance is positive if strike is larger than desired size,
            // or negative if smaller. If previously a found smaller strike,
            // then prefer a larger strike. Otherwise, minimize distance.
            double dist = fFTFace->available_sizes[i].y_ppem / 64.0 - minor;
            if (bestDist < 0 ? dist >= bestDist : fabs(dist) <= bestDist) {
                bestDist = dist;
                bestSize = i;
            }
        }
        if (bestSize < 0) {
            fScaleX = fRec.fTextSize * fRec.fPreScaleX;
            fScaleY = fRec.fTextSize;
            fHaveShape = false;
            return false;
        }
        major = fFTFace->available_sizes[bestSize].x_ppem / 64.0;
        minor = fFTFace->available_sizes[bestSize].y_ppem / 64.0;
        fHaveShape = true;
    } else {
        fHaveShape = !m.isScaleTranslate() || scaleX < 0.0 || scaleY < 0.0;
    }

    fScaleX = SkDoubleToScalar(major);
    fScaleY = SkDoubleToScalar(minor);

    if (fHaveShape) {
        // Normalize the transform and convert to fixed-point.
        fShapeMatrix = m;
        fShapeMatrix.preScale(SkDoubleToScalar(1.0 / major), SkDoubleToScalar(1.0 / minor));

        fShapeMatrixFT.xx = SkScalarToFixed(fShapeMatrix.getScaleX());
        fShapeMatrixFT.yx = SkScalarToFixed(-fShapeMatrix.getSkewY());
        fShapeMatrixFT.xy = SkScalarToFixed(-fShapeMatrix.getSkewX());
        fShapeMatrixFT.yy = SkScalarToFixed(fShapeMatrix.getScaleY());
    }
    return true;
}

unsigned SkScalerContext_CairoFT::generateGlyphCount()
{
    return fFTFace->num_glyphs;
}

bool SkScalerContext_CairoFT::generateAdvance(SkGlyph* glyph)
{
    generateMetrics(glyph);
    return !glyph->isEmpty();
}

void SkScalerContext_CairoFT::prepareGlyph(FT_GlyphSlot glyph)
{
    if (fRec.fFlags & SkScalerContext::kEmbolden_Flag &&
        gGlyphSlotEmbolden) {
        gGlyphSlotEmbolden(glyph);
    }
}

void SkScalerContext_CairoFT::generateMetrics(SkGlyph* glyph)
{
    glyph->fMaskFormat = fRec.fMaskFormat;

    glyph->zeroMetrics();

    AutoLockFTFace faceLock(this);

    FT_Error err = mozilla_LoadFTGlyph(fFTFace, glyph->getGlyphID(), fLoadGlyphFlags);
    if (err != 0) {
        return;
    }

    prepareGlyph(fFTFace->glyph);

    glyph->fAdvanceX = SkFDot6ToFloat(fFTFace->glyph->advance.x);
    glyph->fAdvanceY = -SkFDot6ToFloat(fFTFace->glyph->advance.y);

    SkIRect bounds;
    switch (fFTFace->glyph->format) {
    case FT_GLYPH_FORMAT_OUTLINE:
        if (!fFTFace->glyph->outline.n_contours) {
            return;
        }

        FT_BBox bbox;
        FT_Outline_Get_CBox(&fFTFace->glyph->outline, &bbox);
        if (this->isSubpixel()) {
            int dx = SkFixedToFDot6(glyph->getSubXFixed());
            int dy = SkFixedToFDot6(glyph->getSubYFixed());
            bbox.xMin += dx;
            bbox.yMin -= dy;
            bbox.xMax += dx;
            bbox.yMax -= dy;
        }
        bbox.xMin &= ~63;
        bbox.yMin &= ~63;
        bbox.xMax = (bbox.xMax + 63) & ~63;
        bbox.yMax = (bbox.yMax + 63) & ~63;
        bounds = SkIRect::MakeLTRB(SkFDot6Floor(bbox.xMin),
                                   -SkFDot6Floor(bbox.yMax),
                                   SkFDot6Floor(bbox.xMax),
                                   -SkFDot6Floor(bbox.yMin));

        if (isLCD(fRec)) {
            // In FreeType < 2.8.1, LCD filtering, if explicitly used, may
            // add padding to the glyph. When not used, there is no padding.
            // As of 2.8.1, LCD filtering is now always supported and may
            // add padding even if an LCD filter is not explicitly set.
            // Regardless, if no LCD filtering is used, or if LCD filtering
            // doesn't add padding, it is safe to modify the glyph's bounds
            // here. generateGlyphImage will detect if the mask is smaller
            // than the bounds and clip things appropriately.
            if (fRec.fFlags & kLCD_Vertical_Flag) {
                bounds.outset(0, 1);
            } else {
                bounds.outset(1, 0);
            }
        }
        break;
    case FT_GLYPH_FORMAT_BITMAP:
        if (fFTFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
            glyph->fMaskFormat = SkMask::kARGB32_Format;
        }

        if (isLCD(fRec)) {
            fRec.fMaskFormat = SkMask::kA8_Format;
        }

        if (fHaveShape) {
            // Ensure filtering is preserved when the bitmap is transformed.
            // Otherwise, the result will look horrifically aliased.
            if (fRec.fMaskFormat == SkMask::kBW_Format) {
                fRec.fMaskFormat = SkMask::kA8_Format;
            }

            // Apply the shape matrix to the glyph's bounding box.
            SkRect srcRect = SkRect::MakeXYWH(
                SkIntToScalar(fFTFace->glyph->bitmap_left),
                -SkIntToScalar(fFTFace->glyph->bitmap_top),
                SkIntToScalar(fFTFace->glyph->bitmap.width),
                SkIntToScalar(fFTFace->glyph->bitmap.rows));
            SkRect destRect;
            fShapeMatrix.mapRect(&destRect, srcRect);
            SkIRect glyphRect = destRect.roundOut();
            bounds = SkIRect::MakeXYWH(SkScalarRoundToInt(destRect.fLeft),
                                       SkScalarRoundToInt(destRect.fTop),
                                       glyphRect.width(),
                                       glyphRect.height());
        } else {
            bounds = SkIRect::MakeXYWH(fFTFace->glyph->bitmap_left,
                                       -fFTFace->glyph->bitmap_top,
                                       fFTFace->glyph->bitmap.width,
                                       fFTFace->glyph->bitmap.rows);
        }
        break;
    default:
        SkDEBUGFAIL("unknown glyph format");
        return;
    }

    if (SkIRect::MakeXYWH(SHRT_MIN, SHRT_MIN, USHRT_MAX, USHRT_MAX).contains(bounds)) {
        glyph->fWidth  = SkToU16(bounds.width());
        glyph->fHeight = SkToU16(bounds.height());
        glyph->fLeft   = SkToS16(bounds.left());
        glyph->fTop    = SkToS16(bounds.top());
    }
}

void SkScalerContext_CairoFT::generateImage(const SkGlyph& glyph)
{
    AutoLockFTFace faceLock(this);

    FT_Error err = mozilla_LoadFTGlyph(fFTFace, glyph.getGlyphID(), fLoadGlyphFlags);

    if (err != 0) {
        memset(glyph.fImage, 0, glyph.rowBytes() * glyph.fHeight);
        return;
    }

    prepareGlyph(fFTFace->glyph);

    bool useLcdFilter =
        fFTFace->glyph->format == FT_GLYPH_FORMAT_OUTLINE &&
        glyph.maskFormat() == SkMask::kLCD16_Format &&
        gSetLcdFilter;
    if (useLcdFilter) {
        mozilla_LockFTLibrary(fFTFace->glyph->library);
        gSetLcdFilter(fFTFace->glyph->library, fLcdFilter);
    }

    SkMatrix matrix;
    if (fFTFace->glyph->format == FT_GLYPH_FORMAT_BITMAP &&
        fHaveShape) {
        matrix = fShapeMatrix;
    } else {
        matrix.setIdentity();
    }
    generateGlyphImage(fFTFace, glyph, matrix);

    if (useLcdFilter) {
        gSetLcdFilter(fFTFace->glyph->library, FT_LCD_FILTER_NONE);
        mozilla_UnlockFTLibrary(fFTFace->glyph->library);
    }
}

bool SkScalerContext_CairoFT::generatePath(SkGlyphID glyphID, SkPath* path)
{
    AutoLockFTFace faceLock(this);

    SkASSERT(path);

    uint32_t flags = fLoadGlyphFlags;
    flags |= FT_LOAD_NO_BITMAP; // ignore embedded bitmaps so we're sure to get the outline
    flags &= ~FT_LOAD_RENDER;   // don't scan convert (we just want the outline)

    FT_Error err = mozilla_LoadFTGlyph(fFTFace, glyphID, flags);

    if (err != 0) {
        path->reset();
        return false;
    }

    prepareGlyph(fFTFace->glyph);

    return generateGlyphPath(fFTFace, path);
}

void SkScalerContext_CairoFT::generateFontMetrics(SkFontMetrics* metrics)
{
    if (metrics) {
        memset(metrics, 0, sizeof(SkFontMetrics));
    }
}

///////////////////////////////////////////////////////////////////////////////

#include "include/core/SkFontMgr.h"

sk_sp<SkFontMgr> SkFontMgr::Factory() {
    // todo
    return nullptr;
}

