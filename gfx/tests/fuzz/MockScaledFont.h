/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FUZZ_MOCKSCALEDFONT_H
#define FUZZ_MOCKSCALEDFONT_H

#include "mozilla/gfx/2D.h"

class MockUnscaledFont : public mozilla::gfx::UnscaledFont {
 public:
  using FontType = mozilla::gfx::FontType;

  FontType GetType() const final { return FontType::UNKNOWN; }
};

class MockScaledFont : public mozilla::gfx::ScaledFont {
 public:
  using FontType = mozilla::gfx::FontType;
  using Float = mozilla::gfx::Float;
  using Path = mozilla::gfx::Path;
  using GlyphBuffer = mozilla::gfx::GlyphBuffer;
  using DrawTarget = mozilla::gfx::DrawTarget;
  using PathBuilder = mozilla::gfx::PathBuilder;
  using Matrix = mozilla::gfx::Matrix;

  MockScaledFont(const RefPtr<MockUnscaledFont>& aUnscaledFont,
                 hb_font_t* aHBFont)
      : ScaledFont(aUnscaledFont), mHBFont(hb_font_reference(aHBFont)) {}
  virtual ~MockScaledFont() { hb_font_destroy(mHBFont); }

  FontType GetType() const final { return FontType::UNKNOWN; }
  Float GetSize() const final {
    int x, y;
    hb_font_get_scale(mHBFont, &x, &y);
    return Float(y / 65536.0);
  }
  already_AddRefed<Path> GetPathForGlyphs(const GlyphBuffer& aBuffer,
                                          const DrawTarget* aTarget) final {
    RefPtr builder = mozilla::gfx::Factory::CreateSimplePathBuilder();
    CopyGlyphsToBuilder(aBuffer, builder);
    RefPtr path = builder->Finish();
    return path.forget();
  }
  void CopyGlyphsToBuilder(const GlyphBuffer& aBuffer, PathBuilder* aBuilder,
                           const Matrix* aTransformHint = nullptr) final {
    // We could use hb_font_get_glyph_shape to extract the glyph path here,
    // but the COLRv1 parsing code doesn't actually use it (it just passes it
    // through to Moz2D), so for now we'll just return an empty path.
  }

 private:
  hb_font_t* mHBFont;
};

#endif
