/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontBase.h"

#include "PathSkia.h"
#include "skia/include/core/SkFont.h"

#ifdef USE_CAIRO
#  include "PathCairo.h"
#  include "DrawTargetCairo.h"
#  include "HelpersCairo.h"
#endif

#include <vector>
#include <cmath>

namespace mozilla {
namespace gfx {

Atomic<uint32_t> UnscaledFont::sDeletionCounter(0);

UnscaledFont::~UnscaledFont() { sDeletionCounter++; }

Atomic<uint32_t> ScaledFont::sDeletionCounter(0);

ScaledFont::~ScaledFont() { sDeletionCounter++; }

ScaledFontBase::~ScaledFontBase() {
  SkSafeUnref<SkTypeface>(mTypeface);
  cairo_scaled_font_destroy(mScaledFont);
}

ScaledFontBase::ScaledFontBase(const RefPtr<UnscaledFont>& aUnscaledFont,
                               Float aSize)
    : ScaledFont(aUnscaledFont),
      mTypeface(nullptr),
      mScaledFont(nullptr),
      mSize(aSize) {}

SkTypeface* ScaledFontBase::GetSkTypeface() {
  if (!mTypeface) {
    SkTypeface* typeface = CreateSkTypeface();
    if (!mTypeface.compareExchange(nullptr, typeface)) {
      SkSafeUnref(typeface);
    }
  }
  return mTypeface;
}

cairo_scaled_font_t* ScaledFontBase::GetCairoScaledFont() {
  if (mScaledFont) {
    return mScaledFont;
  }

  cairo_font_options_t* fontOptions = cairo_font_options_create();
  cairo_font_face_t* fontFace = CreateCairoFontFace(fontOptions);
  if (!fontFace) {
    cairo_font_options_destroy(fontOptions);
    return nullptr;
  }

  cairo_matrix_t sizeMatrix;
  cairo_matrix_t identityMatrix;

  cairo_matrix_init_scale(&sizeMatrix, mSize, mSize);
  cairo_matrix_init_identity(&identityMatrix);

  cairo_scaled_font_t* scaledFont = cairo_scaled_font_create(
      fontFace, &sizeMatrix, &identityMatrix, fontOptions);

  cairo_font_options_destroy(fontOptions);
  cairo_font_face_destroy(fontFace);

  if (cairo_scaled_font_status(scaledFont) != CAIRO_STATUS_SUCCESS) {
    cairo_scaled_font_destroy(scaledFont);
    return nullptr;
  }

  PrepareCairoScaledFont(scaledFont);
  mScaledFont = scaledFont;
  return mScaledFont;
}

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

already_AddRefed<Path> ScaledFontBase::GetPathForGlyphs(
    const GlyphBuffer& aBuffer, const DrawTarget* aTarget) {
  if (aTarget->GetBackendType() == BackendType::SKIA) {
    SkPath path = GetSkiaPathForGlyphs(aBuffer);
    return MakeAndAddRef<PathSkia>(path, FillRule::FILL_WINDING);
  }
#ifdef USE_CAIRO
  if (aTarget->GetBackendType() == BackendType::CAIRO) {
    auto* cairoScaledFont = GetCairoScaledFont();
    if (!cairoScaledFont) {
      MOZ_ASSERT_UNREACHABLE("Invalid scaled font");
      return nullptr;
    }

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

    cairo_set_scaled_font(ctx, cairoScaledFont);

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
  RefPtr<PathBuilder> builder = aTarget->CreatePathBuilder();
  SkPath skPath = GetSkiaPathForGlyphs(aBuffer);
  RefPtr<Path> path = MakeAndAddRef<PathSkia>(skPath, FillRule::FILL_WINDING);
  path->StreamToSink(builder);
  return builder->Finish();
}

void ScaledFontBase::CopyGlyphsToBuilder(const GlyphBuffer& aBuffer,
                                         PathBuilder* aBuilder,
                                         const Matrix* aTransformHint) {
  BackendType backendType = aBuilder->GetBackendType();
  if (backendType == BackendType::SKIA) {
    PathBuilderSkia* builder = static_cast<PathBuilderSkia*>(aBuilder);
    builder->AppendPath(GetSkiaPathForGlyphs(aBuffer));
    return;
  }
#ifdef USE_CAIRO
  if (backendType == BackendType::CAIRO) {
    auto* cairoScaledFont = GetCairoScaledFont();
    if (!cairoScaledFont) {
      MOZ_ASSERT_UNREACHABLE("Invalid scaled font");
      return;
    }

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

    cairo_set_scaled_font(ctx, cairoScaledFont);
    cairo_glyph_path(ctx, &glyphs[0], aBuffer.mNumGlyphs);

    RefPtr<PathCairo> cairoPath = new PathCairo(ctx);
    cairo_destroy(ctx);

    cairoPath->AppendPathToBuilder(builder);
    return;
  }
#endif
  if (backendType == BackendType::RECORDING) {
    SkPath skPath = GetSkiaPathForGlyphs(aBuffer);
    RefPtr<Path> path = MakeAndAddRef<PathSkia>(skPath, FillRule::FILL_WINDING);
    path->StreamToSink(aBuilder);
    return;
  }
  MOZ_ASSERT(false, "Path not being copied");
}

}  // namespace gfx
}  // namespace mozilla
