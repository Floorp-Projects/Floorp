/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontBase.h"

#include "gfxPrefs.h"

#ifdef USE_SKIA
#  include "PathSkia.h"
#  include "skia/include/core/SkFont.h"
#endif

#ifdef USE_CAIRO
#  include "PathCairo.h"
#  include "DrawTargetCairo.h"
#  include "HelpersCairo.h"
#endif

#include <vector>
#include <cmath>

using namespace std;

namespace mozilla {
namespace gfx {

Atomic<uint32_t> UnscaledFont::sDeletionCounter(0);

UnscaledFont::~UnscaledFont() { sDeletionCounter++; }

Atomic<uint32_t> ScaledFont::sDeletionCounter(0);

ScaledFont::~ScaledFont() { sDeletionCounter++; }

AntialiasMode ScaledFont::GetDefaultAAMode() {
  if (gfxPrefs::DisableAllTextAA()) {
    return AntialiasMode::NONE;
  }

  return AntialiasMode::DEFAULT;
}

ScaledFontBase::~ScaledFontBase() {
#ifdef USE_SKIA
  SkSafeUnref<SkTypeface>(mTypeface);
#endif
#ifdef USE_CAIRO_SCALED_FONT
  cairo_scaled_font_destroy(mScaledFont);
#endif
}

ScaledFontBase::ScaledFontBase(const RefPtr<UnscaledFont>& aUnscaledFont,
                               Float aSize)
    : ScaledFont(aUnscaledFont)
#ifdef USE_SKIA
      ,
      mTypeface(nullptr)
#endif
#ifdef USE_CAIRO_SCALED_FONT
      ,
      mScaledFont(nullptr)
#endif
      ,
      mSize(aSize) {
}

#ifdef USE_SKIA
SkTypeface* ScaledFontBase::GetSkTypeface() {
  if (!mTypeface) {
    SkTypeface* typeface = CreateSkTypeface();
    if (!mTypeface.compareExchange(nullptr, typeface)) {
      SkSafeUnref(typeface);
    }
  }
  return mTypeface;
}
#endif

#ifdef USE_CAIRO_SCALED_FONT
bool ScaledFontBase::PopulateCairoScaledFont() {
  cairo_font_face_t* cairoFontFace = GetCairoFontFace();
  if (!cairoFontFace) {
    return false;
  }

  cairo_matrix_t sizeMatrix;
  cairo_matrix_t identityMatrix;

  cairo_matrix_init_scale(&sizeMatrix, mSize, mSize);
  cairo_matrix_init_identity(&identityMatrix);

  cairo_font_options_t* fontOptions = cairo_font_options_create();

  mScaledFont = cairo_scaled_font_create(cairoFontFace, &sizeMatrix,
                                         &identityMatrix, fontOptions);

  cairo_font_options_destroy(fontOptions);
  cairo_font_face_destroy(cairoFontFace);

  return (cairo_scaled_font_status(mScaledFont) == CAIRO_STATUS_SUCCESS);
}
#endif

#ifdef USE_SKIA
SkPath ScaledFontBase::GetSkiaPathForGlyphs(const GlyphBuffer& aBuffer) {
  SkTypeface* typeFace = GetSkTypeface();
  MOZ_ASSERT(typeFace);

  SkFont font(sk_ref_sp(typeFace), SkFloatToScalar(mSize));

  std::vector<uint16_t> indices;
  indices.resize(aBuffer.mNumGlyphs);
  for (unsigned int i = 0; i < aBuffer.mNumGlyphs; i++) {
    indices[i] = aBuffer.mGlyphs[i].mIndex;
  }

  struct Context {
    const Glyph* mGlyph;
    SkPath mPath;
  } ctx = {aBuffer.mGlyphs};

  font.getPaths(
      indices.data(), indices.size(),
      [](const SkPath* glyphPath, const SkMatrix& scaleMatrix, void* ctxPtr) {
        Context& ctx = *reinterpret_cast<Context*>(ctxPtr);
        if (glyphPath) {
          SkMatrix transMatrix(scaleMatrix);
          transMatrix.postTranslate(SkFloatToScalar(ctx.mGlyph->mPosition.x),
                                    SkFloatToScalar(ctx.mGlyph->mPosition.y));
          ctx.mPath.addPath(*glyphPath, transMatrix);
        }
        ++ctx.mGlyph;
      },
      &ctx);

  return ctx.mPath;
}
#endif

already_AddRefed<Path> ScaledFontBase::GetPathForGlyphs(
    const GlyphBuffer& aBuffer, const DrawTarget* aTarget) {
#ifdef USE_SKIA
  if (aTarget->GetBackendType() == BackendType::SKIA) {
    SkPath path = GetSkiaPathForGlyphs(aBuffer);
    return MakeAndAddRef<PathSkia>(path, FillRule::FILL_WINDING);
  }
#endif
#ifdef USE_CAIRO
  if (aTarget->GetBackendType() == BackendType::CAIRO) {
    MOZ_ASSERT(mScaledFont);

    DrawTarget* dt = const_cast<DrawTarget*>(aTarget);
    cairo_t* ctx = static_cast<cairo_t*>(
        dt->GetNativeSurface(NativeSurfaceType::CAIRO_CONTEXT));

    bool isNewContext = !ctx;
    if (!ctx) {
      ctx = cairo_create(DrawTargetCairo::GetDummySurface());
      cairo_matrix_t mat;
      GfxMatrixToCairoMatrix(aTarget->GetTransform(), mat);
      cairo_set_matrix(ctx, &mat);
    }

    cairo_set_scaled_font(ctx, mScaledFont);

    // Convert our GlyphBuffer into an array of Cairo glyphs.
    std::vector<cairo_glyph_t> glyphs(aBuffer.mNumGlyphs);
    for (uint32_t i = 0; i < aBuffer.mNumGlyphs; ++i) {
      glyphs[i].index = aBuffer.mGlyphs[i].mIndex;
      glyphs[i].x = aBuffer.mGlyphs[i].mPosition.x;
      glyphs[i].y = aBuffer.mGlyphs[i].mPosition.y;
    }

    cairo_new_path(ctx);

    cairo_glyph_path(ctx, &glyphs[0], aBuffer.mNumGlyphs);

    RefPtr<PathCairo> newPath = new PathCairo(ctx);
    if (isNewContext) {
      cairo_destroy(ctx);
    }

    return newPath.forget();
  }
#endif
#ifdef USE_SKIA
  RefPtr<PathBuilder> builder = aTarget->CreatePathBuilder();
  SkPath skPath = GetSkiaPathForGlyphs(aBuffer);
  RefPtr<Path> path = MakeAndAddRef<PathSkia>(skPath, FillRule::FILL_WINDING);
  path->StreamToSink(builder);
  return builder->Finish();
#endif
  return nullptr;
}

void ScaledFontBase::CopyGlyphsToBuilder(const GlyphBuffer& aBuffer,
                                         PathBuilder* aBuilder,
                                         const Matrix* aTransformHint) {
  BackendType backendType = aBuilder->GetBackendType();
#ifdef USE_SKIA
  if (backendType == BackendType::SKIA) {
    PathBuilderSkia* builder = static_cast<PathBuilderSkia*>(aBuilder);
    builder->AppendPath(GetSkiaPathForGlyphs(aBuffer));
    return;
  }
#endif
#ifdef USE_CAIRO
  if (backendType == BackendType::CAIRO) {
    MOZ_ASSERT(mScaledFont);

    PathBuilderCairo* builder = static_cast<PathBuilderCairo*>(aBuilder);
    cairo_t* ctx = cairo_create(DrawTargetCairo::GetDummySurface());

    if (aTransformHint) {
      cairo_matrix_t mat;
      GfxMatrixToCairoMatrix(*aTransformHint, mat);
      cairo_set_matrix(ctx, &mat);
    }

    // Convert our GlyphBuffer into an array of Cairo glyphs.
    std::vector<cairo_glyph_t> glyphs(aBuffer.mNumGlyphs);
    for (uint32_t i = 0; i < aBuffer.mNumGlyphs; ++i) {
      glyphs[i].index = aBuffer.mGlyphs[i].mIndex;
      glyphs[i].x = aBuffer.mGlyphs[i].mPosition.x;
      glyphs[i].y = aBuffer.mGlyphs[i].mPosition.y;
    }

    cairo_set_scaled_font(ctx, mScaledFont);
    cairo_glyph_path(ctx, &glyphs[0], aBuffer.mNumGlyphs);

    RefPtr<PathCairo> cairoPath = new PathCairo(ctx);
    cairo_destroy(ctx);

    cairoPath->AppendPathToBuilder(builder);
    return;
  }
#endif
#ifdef USE_SKIA
  if (backendType == BackendType::RECORDING) {
    SkPath skPath = GetSkiaPathForGlyphs(aBuffer);
    RefPtr<Path> path = MakeAndAddRef<PathSkia>(skPath, FillRule::FILL_WINDING);
    path->StreamToSink(aBuilder);
    return;
  }
#endif
  MOZ_ASSERT(false, "Path not being copied");
}

void ScaledFontBase::GetGlyphDesignMetrics(const uint16_t* aGlyphs,
                                           uint32_t aNumGlyphs,
                                           GlyphMetrics* aGlyphMetrics) {
#ifdef USE_CAIRO_SCALED_FONT
  if (mScaledFont) {
    for (uint32_t i = 0; i < aNumGlyphs; i++) {
      cairo_glyph_t glyph;
      cairo_text_extents_t extents;
      glyph.index = aGlyphs[i];
      glyph.x = 0;
      glyph.y = 0;

      cairo_scaled_font_glyph_extents(mScaledFont, &glyph, 1, &extents);

      aGlyphMetrics[i].mXBearing = extents.x_bearing;
      aGlyphMetrics[i].mXAdvance = extents.x_advance;
      aGlyphMetrics[i].mYBearing = extents.y_bearing;
      aGlyphMetrics[i].mYAdvance = extents.y_advance;
      aGlyphMetrics[i].mWidth = extents.width;
      aGlyphMetrics[i].mHeight = extents.height;

      cairo_font_options_t* options = cairo_font_options_create();
      cairo_scaled_font_get_font_options(mScaledFont, options);

      if (cairo_font_options_get_antialias(options) != CAIRO_ANTIALIAS_NONE) {
        if (cairo_scaled_font_get_type(mScaledFont) == CAIRO_FONT_TYPE_WIN32) {
          if (aGlyphMetrics[i].mWidth > 0 && aGlyphMetrics[i].mHeight > 0) {
            aGlyphMetrics[i].mWidth -= 3.0f;
            aGlyphMetrics[i].mXBearing += 1.0f;
          }
        }
#  if defined(MOZ2D_HAS_MOZ_CAIRO) && defined(CAIRO_HAS_DWRITE_FONT)
        else if (cairo_scaled_font_get_type(mScaledFont) ==
                 CAIRO_FONT_TYPE_DWRITE) {
          if (aGlyphMetrics[i].mWidth > 0 && aGlyphMetrics[i].mHeight > 0) {
            aGlyphMetrics[i].mWidth -= 2.0f;
            aGlyphMetrics[i].mXBearing += 1.0f;
          }
        }
#  endif
      }
      cairo_font_options_destroy(options);
    }
    return;
  }
#endif

  // Don't know how to get the glyph metrics...
  MOZ_CRASH(
      "The specific backend type is not supported for GetGlyphDesignMetrics.");
}

#ifdef USE_CAIRO_SCALED_FONT
void ScaledFontBase::SetCairoScaledFont(cairo_scaled_font_t* font) {
  MOZ_ASSERT(!mScaledFont);

  if (font == mScaledFont) return;

  if (mScaledFont) cairo_scaled_font_destroy(mScaledFont);

  mScaledFont = font;
  cairo_scaled_font_reference(mScaledFont);
}
#endif

}  // namespace gfx
}  // namespace mozilla
