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
    return nsnull;
}

already_AddRefed<GLContext>
GLContextProviderNull::CreateOffscreen(const gfxIntSize&,
                                       const ContextFormat&,
                                       const ContextFlags)
{
    return nsnull;
}

already_AddRefed<GLContext>
GLContextProviderNull::CreateForNativePixmapSurface(gfxASurface *)
{
    return nsnull;
}

GLContext *
GLContextProviderNull::GetGlobalContext()
{
    return nsnull;
}

void
GLContextProviderNull::Shutdown()
{
}

} /* namespace gl */
} /* namespace mozilla */
