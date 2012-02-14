/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Woodrow <mwoodrow@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "ScaledFontBase.h"

#include "gfxFont.h"

#ifdef USE_SKIA
#include "PathSkia.h"
#include "skia/SkPaint.h"
#include "skia/SkPath.h"
#endif

#ifdef USE_CAIRO
#include "PathCairo.h"
#endif

#include <vector>
#include <cmath>

using namespace std;

namespace mozilla {
namespace gfx {

ScaledFontBase::~ScaledFontBase()
{
#ifdef USE_SKIA
  SkSafeUnref(mTypeface);
#endif
#ifdef USE_CAIRO
  cairo_scaled_font_destroy(mScaledFont);
#endif
}

ScaledFontBase::ScaledFontBase(Float aSize)
  : mSize(aSize)
{
#ifdef USE_SKIA
  mTypeface = NULL;
#endif
#ifdef USE_CAIRO
  mScaledFont = NULL;
#endif
}

TemporaryRef<Path>
ScaledFontBase::GetPathForGlyphs(const GlyphBuffer &aBuffer, const DrawTarget *aTarget)
{
#ifdef USE_SKIA
  if (aTarget->GetType() == BACKEND_SKIA) {
    SkPaint paint;
    paint.setTypeface(GetSkTypeface());
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    paint.setTextSize(SkFloatToScalar(mSize));

    std::vector<uint16_t> indices;
    std::vector<SkPoint> offsets;
    indices.resize(aBuffer.mNumGlyphs);
    offsets.resize(aBuffer.mNumGlyphs);

    for (unsigned int i = 0; i < aBuffer.mNumGlyphs; i++) {
      indices[i] = aBuffer.mGlyphs[i].mIndex;
      offsets[i].fX = SkFloatToScalar(aBuffer.mGlyphs[i].mPosition.x);
      offsets[i].fY = SkFloatToScalar(aBuffer.mGlyphs[i].mPosition.y);
    }

    SkPath path;
    paint.getPosTextPath(&indices.front(), aBuffer.mNumGlyphs*2, &offsets.front(), &path);
    return new PathSkia(path, FILL_WINDING);
  }
#endif
#ifdef USE_CAIRO
  if (aTarget->GetType() == BACKEND_CAIRO) {
    MOZ_ASSERT(mScaledFont);

    RefPtr<PathBuilder> builder_iface = aTarget->CreatePathBuilder();
    PathBuilderCairo* builder = static_cast<PathBuilderCairo*>(builder_iface.get());

    // Manually build the path for the PathBuilder.
    RefPtr<CairoPathContext> context = builder->GetPathContext();

    cairo_set_scaled_font(*context, mScaledFont);

    // Convert our GlyphBuffer into an array of Cairo glyphs.
    std::vector<cairo_glyph_t> glyphs(aBuffer.mNumGlyphs);
    for (uint32_t i = 0; i < aBuffer.mNumGlyphs; ++i) {
      glyphs[i].index = aBuffer.mGlyphs[i].mIndex;
      glyphs[i].x = aBuffer.mGlyphs[i].mPosition.x;
      glyphs[i].y = aBuffer.mGlyphs[i].mPosition.y;
    }

    cairo_glyph_path(*context, &glyphs[0], aBuffer.mNumGlyphs);

    return builder->Finish();
  }
#endif
  return NULL;
}

#ifdef USE_CAIRO
void
ScaledFontBase::SetCairoScaledFont(cairo_scaled_font_t* font)
{
  MOZ_ASSERT(!mScaledFont);

  mScaledFont = font;
  cairo_scaled_font_reference(mScaledFont);
}
#endif

}
}
