/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderTextLayer.h"
#include "WebRenderLayersLogging.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/WebRenderBridgeChild.h"

#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

static void
DWriteFontFileData(const uint8_t* aData, uint32_t aLength, uint32_t aIndex,
                   float aGlyphSize, uint32_t aVariationCount,
                   const ScaledFont::VariationSetting* aVariations, void* aBaton)
{
    WebRenderTextLayer* layer = static_cast<WebRenderTextLayer*>(aBaton);

    uint8_t* fontData = (uint8_t*)malloc(aLength * sizeof(uint8_t));
    memcpy(fontData, aData, aLength * sizeof(uint8_t));

    layer->mFontData = fontData;
    layer->mFontDataLength = aLength;
    layer->mIndex = aIndex;
    layer->mGlyphSize = aGlyphSize;
}

void
WebRenderTextLayer::RenderLayer()
{
    if (mBounds.IsEmpty()) {
        return;
    }

    gfx::Rect rect = RelativeToParent(GetTransform().TransformBounds(IntRectToRect(mBounds)));
    gfx::Rect clip;
    if (GetClipRect().isSome()) {
      clip = RelativeToParent(IntRectToRect(GetClipRect().ref().ToUnknownRect()));
    } else {
      clip = rect;
    }

    MOZ_ASSERT(mFont->GetType() == FontType::DWRITE);
    mFont->GetFontFileData(&DWriteFontFileData, this);
    wr::ByteBuffer fontBuffer(mFontDataLength, mFontData);

    nsTArray<WrGlyphArray> wr_glyphs;
    wr_glyphs.SetLength(mGlyphs.Length());

    for (size_t i = 0; i < mGlyphs.Length(); i++) {
        GlyphArray glyph_array = mGlyphs[i];
        nsTArray<Glyph>& glyphs = glyph_array.glyphs();

        nsTArray<WrGlyphInstance>& wr_glyph_instances = wr_glyphs[i].glyphs;
        wr_glyph_instances.SetLength(glyphs.Length());
        wr_glyphs[i].color = glyph_array.color().value();

        for (size_t j = 0; j < glyphs.Length(); j++) {
            wr_glyph_instances[j].index = glyphs[j].mIndex;
            wr_glyph_instances[j].x = glyphs[j].mPosition.x;
            wr_glyph_instances[j].y = glyphs[j].mPosition.y;
        }
    }

    if (gfxPrefs::LayersDump()) {
        printf_stderr("TextLayer %p using rect=%s, clip=%s\n",
                      this->GetLayer(),
                      Stringify(rect).c_str(),
                      Stringify(clip).c_str());
    }

    WrBridge()->AddWebRenderCommand(OpDPPushText(
        wr::ToWrRect(rect),
        wr::ToWrRect(clip),
        wr_glyphs,
        mIndex,
        mGlyphSize,
        fontBuffer,
        mFontDataLength
    ));
}

} // namespace layers
} // namespace mozilla
