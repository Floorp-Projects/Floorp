
/*
 * Copyright 2012 Mozilla Foundation
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cairo.h"
#include "cairo-ft.h"

#include "SkFontHost_FreeType_common.h"

#include "SkAdvancedTypefaceMetrics.h"
#include "SkFDot6.h"
#include "SkFontHost.h"
#include "SkPath.h"
#include "SkScalerContext.h"
#include "SkTypefaceCache.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#ifndef SK_FONTHOST_CAIRO_STANDALONE
#define SK_FONTHOST_CAIRO_STANDALONE 1
#endif

static cairo_user_data_key_t kSkTypefaceKey;

class SkScalerContext_CairoFT : public SkScalerContext_FreeType_Base {
public:
    SkScalerContext_CairoFT(SkTypeface* typeface, const SkDescriptor* desc);
    virtual ~SkScalerContext_CairoFT();

protected:
    virtual unsigned generateGlyphCount() override;
    virtual uint16_t generateCharToGlyph(SkUnichar uniChar) override;
    virtual void generateAdvance(SkGlyph* glyph) override;
    virtual void generateMetrics(SkGlyph* glyph) override;
    virtual void generateImage(const SkGlyph& glyph) override;
    virtual void generatePath(const SkGlyph& glyph, SkPath* path) override;
    virtual void generateFontMetrics(SkPaint::FontMetrics* metrics) override;
    virtual SkUnichar generateGlyphToChar(uint16_t glyph) override;
private:
    cairo_scaled_font_t* fScaledFont;
    uint32_t fLoadGlyphFlags;
};

class CairoLockedFTFace {
public:
    CairoLockedFTFace(cairo_scaled_font_t* scaledFont)
        : fScaledFont(scaledFont)
        , fFace(cairo_ft_scaled_font_lock_face(scaledFont))
    {}

    ~CairoLockedFTFace()
    {
        cairo_ft_scaled_font_unlock_face(fScaledFont);
    }

    FT_Face getFace()
    {
        return fFace;
    }

private:
    cairo_scaled_font_t* fScaledFont;
    FT_Face fFace;
};

static bool bothZero(SkScalar a, SkScalar b) {
    return 0 == a && 0 == b;
}

// returns false if there is any non-90-rotation or skew
static bool isAxisAligned(const SkScalerContext::Rec& rec) {
    return 0 == rec.fPreSkewX &&
           (bothZero(rec.fPost2x2[0][1], rec.fPost2x2[1][0]) ||
            bothZero(rec.fPost2x2[0][0], rec.fPost2x2[1][1]));
}

class SkCairoFTTypeface : public SkTypeface {
public:
    static SkTypeface* CreateTypeface(cairo_font_face_t* fontFace, const SkFontStyle& style, bool isFixedWidth) {
        SkASSERT(fontFace != NULL);
        SkASSERT(cairo_font_face_get_type(fontFace) == CAIRO_FONT_TYPE_FT);

        SkFontID newId = SkTypefaceCache::NewFontID();

        return new SkCairoFTTypeface(fontFace, style, newId, isFixedWidth);
    }

    cairo_font_face_t* getFontFace() {
        return fFontFace;
    }

    virtual SkStreamAsset* onOpenStream(int*) const override { return NULL; }

    virtual SkAdvancedTypefaceMetrics*
        onGetAdvancedTypefaceMetrics(PerGlyphInfo,
                                     const uint32_t*, uint32_t) const override
    {
        SkDEBUGCODE(SkDebugf("SkCairoFTTypeface::onGetAdvancedTypefaceMetrics unimplemented\n"));
        return NULL;
    }

    virtual SkScalerContext* onCreateScalerContext(const SkDescriptor* desc) const override
    {
        return new SkScalerContext_CairoFT(const_cast<SkCairoFTTypeface*>(this), desc);
    }

    virtual void onFilterRec(SkScalerContextRec* rec) const override
    {
        // rotated text looks bad with hinting, so we disable it as needed
        if (!isAxisAligned(*rec)) {
            rec->setHinting(SkPaint::kNo_Hinting);
        }
    }

    virtual void onGetFontDescriptor(SkFontDescriptor*, bool*) const override
    {
        SkDEBUGCODE(SkDebugf("SkCairoFTTypeface::onGetFontDescriptor unimplemented\n"));
    }

    virtual int onCharsToGlyphs(void const*, SkTypeface::Encoding, uint16_t*, int) const override
    {
        return 0;
    }

    virtual int onCountGlyphs() const override
    {
        return 0;
    }

    virtual int onGetUPEM() const override
    {
        return 0;
    }

    virtual SkTypeface::LocalizedStrings* onCreateFamilyNameIterator() const override
    {
        return NULL;
    }

    virtual void onGetFamilyName(SkString* familyName) const override
    {
        familyName->reset();
    }

    virtual int onGetTableTags(SkFontTableTag*) const override
    {
        return 0;
    }

    virtual size_t onGetTableData(SkFontTableTag, size_t, size_t, void*) const override
    {
        return 0;
    }

private:

    SkCairoFTTypeface(cairo_font_face_t* fontFace, const SkFontStyle& style, SkFontID id, bool isFixedWidth)
        : SkTypeface(style, id, isFixedWidth)
        , fFontFace(fontFace)
    {
        cairo_font_face_set_user_data(fFontFace, &kSkTypefaceKey, this, NULL);
        cairo_font_face_reference(fFontFace);
    }

    ~SkCairoFTTypeface()
    {
        cairo_font_face_set_user_data(fFontFace, &kSkTypefaceKey, NULL, NULL);
        cairo_font_face_destroy(fFontFace);
    }

    cairo_font_face_t* fFontFace;
};

SkTypeface* SkCreateTypefaceFromCairoFont(cairo_font_face_t* fontFace, const SkFontStyle& style, bool isFixedWidth)
{
    SkTypeface* typeface = reinterpret_cast<SkTypeface*>(cairo_font_face_get_user_data(fontFace, &kSkTypefaceKey));

    if (typeface) {
        typeface->ref();
    } else {
        typeface = SkCairoFTTypeface::CreateTypeface(fontFace, style, isFixedWidth);
        SkTypefaceCache::Add(typeface, style);
    }

    return typeface;
}

///////////////////////////////////////////////////////////////////////////////

static bool isLCD(const SkScalerContext::Rec& rec) {
    return SkMask::kLCD16_Format == rec.fMaskFormat;
}

///////////////////////////////////////////////////////////////////////////////
SkScalerContext_CairoFT::SkScalerContext_CairoFT(SkTypeface* typeface, const SkDescriptor* desc)
    : SkScalerContext_FreeType_Base(typeface, desc)
{
    SkMatrix matrix;
    fRec.getSingleMatrix(&matrix);

    cairo_font_face_t* fontFace = static_cast<SkCairoFTTypeface*>(typeface)->getFontFace();

    cairo_matrix_t fontMatrix, ctMatrix;
    cairo_matrix_init(&fontMatrix, matrix.getScaleX(), matrix.getSkewY(), matrix.getSkewX(), matrix.getScaleY(), 0.0, 0.0);
    cairo_matrix_init_scale(&ctMatrix, 1.0, 1.0);

    // We need to ensure that the font options match for hinting, as generateMetrics()
    // uses the fScaledFont which uses these font options
    cairo_font_options_t *fontOptions = cairo_font_options_create();

    FT_Int32 loadFlags = FT_LOAD_DEFAULT;

    if (SkMask::kBW_Format == fRec.fMaskFormat) {
        // See http://code.google.com/p/chromium/issues/detail?id=43252#c24
        loadFlags = FT_LOAD_TARGET_MONO;
        if (fRec.getHinting() == SkPaint::kNo_Hinting) {
            cairo_font_options_set_hint_style(fontOptions, CAIRO_HINT_STYLE_NONE);
            loadFlags = FT_LOAD_NO_HINTING;
        }
    } else {
        switch (fRec.getHinting()) {
        case SkPaint::kNo_Hinting:
            loadFlags = FT_LOAD_NO_HINTING;
            cairo_font_options_set_hint_style(fontOptions, CAIRO_HINT_STYLE_NONE);
            break;
        case SkPaint::kSlight_Hinting:
            loadFlags = FT_LOAD_TARGET_LIGHT;  // This implies FORCE_AUTOHINT
            cairo_font_options_set_hint_style(fontOptions, CAIRO_HINT_STYLE_SLIGHT);
            break;
        case SkPaint::kNormal_Hinting:
            cairo_font_options_set_hint_style(fontOptions, CAIRO_HINT_STYLE_MEDIUM);
            if (fRec.fFlags & SkScalerContext::kForceAutohinting_Flag) {
                loadFlags = FT_LOAD_FORCE_AUTOHINT;
            }
            break;
        case SkPaint::kFull_Hinting:
            cairo_font_options_set_hint_style(fontOptions, CAIRO_HINT_STYLE_FULL);
            if (fRec.fFlags & SkScalerContext::kForceAutohinting_Flag) {
                loadFlags = FT_LOAD_FORCE_AUTOHINT;
            }
            if (isLCD(fRec)) {
                if (SkToBool(fRec.fFlags & SkScalerContext::kLCD_Vertical_Flag)) {
                    loadFlags = FT_LOAD_TARGET_LCD_V;
                } else {
                    loadFlags = FT_LOAD_TARGET_LCD;
                }
            }
            break;
        default:
            SkDebugf("---------- UNKNOWN hinting %d\n", fRec.getHinting());
            break;
        }
    }

    fScaledFont = cairo_scaled_font_create(fontFace, &fontMatrix, &ctMatrix, fontOptions);
    cairo_font_options_destroy(fontOptions);

    if ((fRec.fFlags & SkScalerContext::kEmbeddedBitmapText_Flag) == 0) {
        loadFlags |= FT_LOAD_NO_BITMAP;
    }

    // Always using FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH to get correct
    // advances, as fontconfig and cairo do.
    // See http://code.google.com/p/skia/issues/detail?id=222.
    loadFlags |= FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;

#ifdef FT_LOAD_COLOR
    loadFlags |= FT_LOAD_COLOR;
#endif

    fLoadGlyphFlags = loadFlags;
}

SkScalerContext_CairoFT::~SkScalerContext_CairoFT()
{
    cairo_scaled_font_destroy(fScaledFont);
}

unsigned SkScalerContext_CairoFT::generateGlyphCount()
{
    CairoLockedFTFace faceLock(fScaledFont);
    return faceLock.getFace()->num_glyphs;
}

uint16_t SkScalerContext_CairoFT::generateCharToGlyph(SkUnichar uniChar)
{
    CairoLockedFTFace faceLock(fScaledFont);
    return SkToU16(FT_Get_Char_Index(faceLock.getFace(), uniChar));
}

void SkScalerContext_CairoFT::generateAdvance(SkGlyph* glyph)
{
    generateMetrics(glyph);
}

void SkScalerContext_CairoFT::generateMetrics(SkGlyph* glyph)
{
    SkASSERT(fScaledFont != NULL);

    glyph->zeroMetrics();

    CairoLockedFTFace faceLock(fScaledFont);
    FT_Face face = faceLock.getFace();

    FT_Error err = FT_Load_Glyph( face, glyph->getGlyphID(), fLoadGlyphFlags );
    if (err != 0) {
        return;
    }

    switch (face->glyph->format) {
    case FT_GLYPH_FORMAT_OUTLINE:
        if (!face->glyph->outline.n_contours) {
            break;
        }
        FT_BBox bbox;
        FT_Outline_Get_CBox(&face->glyph->outline, &bbox);
        bbox.xMin &= ~63;
        bbox.yMin &= ~63;
        bbox.xMax = (bbox.xMax + 63) & ~63;
        bbox.yMax = (bbox.yMax + 63) & ~63;
        glyph->fWidth  = SkToU16(SkFDot6Floor(bbox.xMax - bbox.xMin));
        glyph->fHeight = SkToU16(SkFDot6Floor(bbox.yMax - bbox.yMin));
        glyph->fTop    = -SkToS16(SkFDot6Floor(bbox.yMax));
        glyph->fLeft   = SkToS16(SkFDot6Floor(bbox.xMin));
        break;
    case FT_GLYPH_FORMAT_BITMAP:
        glyph->fWidth  = SkToU16(face->glyph->bitmap.width);
        glyph->fHeight = SkToU16(face->glyph->bitmap.rows);
        glyph->fTop    = -SkToS16(face->glyph->bitmap_top);
        glyph->fLeft   = SkToS16(face->glyph->bitmap_left);
        break;
    default:
        SkDEBUGFAIL("unknown glyph format");
        return;
    }

    glyph->fAdvanceX = SkFDot6ToFloat(face->glyph->advance.x);
    glyph->fAdvanceY = -SkFDot6ToFloat(face->glyph->advance.y);
}

void SkScalerContext_CairoFT::generateImage(const SkGlyph& glyph)
{
    SkASSERT(fScaledFont != NULL);
    CairoLockedFTFace faceLock(fScaledFont);
    FT_Face face = faceLock.getFace();

    FT_Error err = FT_Load_Glyph(face, glyph.getGlyphID(), fLoadGlyphFlags);

    if (err != 0) {
        memset(glyph.fImage, 0, glyph.rowBytes() * glyph.fHeight);
        return;
    }

    generateGlyphImage(face, glyph);
}

void SkScalerContext_CairoFT::generatePath(const SkGlyph& glyph, SkPath* path)
{
    SkASSERT(fScaledFont != NULL);
    CairoLockedFTFace faceLock(fScaledFont);
    FT_Face face = faceLock.getFace();

    SkASSERT(&glyph && path);

    uint32_t flags = fLoadGlyphFlags;
    flags |= FT_LOAD_NO_BITMAP; // ignore embedded bitmaps so we're sure to get the outline
    flags &= ~FT_LOAD_RENDER;   // don't scan convert (we just want the outline)

    FT_Error err = FT_Load_Glyph(face, glyph.getGlyphID(), flags);

    if (err != 0) {
        path->reset();
        return;
    }

    generateGlyphPath(face, path);
}

void SkScalerContext_CairoFT::generateFontMetrics(SkPaint::FontMetrics* metrics)
{
    SkDEBUGCODE(SkDebugf("SkScalerContext_CairoFT::generateFontMetrics unimplemented\n"));
}

SkUnichar SkScalerContext_CairoFT::generateGlyphToChar(uint16_t glyph)
{
    SkASSERT(fScaledFont != NULL);
    CairoLockedFTFace faceLock(fScaledFont);
    FT_Face face = faceLock.getFace();

    FT_UInt glyphIndex;
    SkUnichar charCode = FT_Get_First_Char(face, &glyphIndex);
    while (glyphIndex != 0) {
        if (glyphIndex == glyph) {
            return charCode;
        }
        charCode = FT_Get_Next_Char(face, charCode, &glyphIndex);
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////

#include "SkFontMgr.h"

SkFontMgr* SkFontMgr::Factory() {
    // todo
    return NULL;
}

