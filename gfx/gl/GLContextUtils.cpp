/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"
#include "GLContextUtils.h"
#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

TemporaryRef<gfx::DataSourceSurface>
ReadBackSurface(GLContext* aContext, GLuint aTexture, bool aYInvert, SurfaceFormat aFormat)
{
    nsRefPtr<gfxImageSurface> image = aContext->GetTexImage(aTexture, aYInvert, aFormat);
    RefPtr<gfx::DataSourceSurface> surf =
        Factory::CreateDataSourceSurface(gfx::ToIntSize(image->GetSize()), aFormat);

    if (!image->CopyTo(surf)) {
        return nullptr;
    }

    return surf.forget();
}

} /* namespace gl */
} /* namespace mozilla */
