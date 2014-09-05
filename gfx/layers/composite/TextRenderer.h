/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TextRenderer_H
#define GFX_TextRenderer_H

#include "mozilla/gfx/2D.h"
#include "nsISupportsImpl.h"
#include <string>

namespace mozilla {
namespace layers {

class Compositor;

class TextRenderer
{
  ~TextRenderer();

public:
  NS_INLINE_DECL_REFCOUNTING(TextRenderer)

  explicit TextRenderer(Compositor *aCompositor)
    : mCompositor(aCompositor)
  {
  }

  void RenderText(const std::string& aText, const gfx::IntPoint& aOrigin,
                  const gfx::Matrix4x4& aTransform, uint32_t aTextSize,
                  uint32_t aTargetPixelWidth);

  gfx::DataSourceSurface::MappedSurface& GetSurfaceMap() { return mMap; }

private:

  // Note that this may still fail to set mGlyphBitmaps to a valid value
  // if the underlying CreateDataSourceSurface fails for some reason.
  void EnsureInitialized();

  RefPtr<Compositor> mCompositor;
  RefPtr<gfx::DataSourceSurface> mGlyphBitmaps;
  gfx::DataSourceSurface::MappedSurface mMap;
};

}
}

#endif
