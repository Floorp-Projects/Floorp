/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"

namespace mozilla {
namespace gl {

already_AddRefed<GLContext>
GLContextProviderNull::CreateForWindow(nsIWidget*)
{
    return nullptr;
}

already_AddRefed<GLContext>
GLContextProviderNull::CreateOffscreen(const gfxIntSize&,
                                       const SurfaceCaps&,
                                       ContextFlags)
{
    return nullptr;
}

SharedTextureHandle
GLContextProviderNull::CreateSharedHandle(SharedTextureShareType shareType,
                                          void* buffer,
                                          SharedTextureBufferType bufferType)
{
  return 0;
}

already_AddRefed<gfxASurface>
GLContextProviderNull::GetSharedHandleAsSurface(SharedTextureShareType shareType,
                                               SharedTextureHandle sharedHandle)
{
  return nullptr;
}

GLContext*
GLContextProviderNull::GetGlobalContext(ContextFlags)
{
    return nullptr;
}

void
GLContextProviderNull::Shutdown()
{
}

} /* namespace gl */
} /* namespace mozilla */
