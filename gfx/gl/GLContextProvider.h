/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTPROVIDER_H_
#define GLCONTEXTPROVIDER_H_

#include "GLContextTypes.h"
#include "nsAutoPtr.h"
#include "SurfaceTypes.h"

#include "nsSize.h" // for gfxIntSize (needed by GLContextProviderImpl.h below)

class nsIWidget;

namespace mozilla {
namespace gl {

#define IN_GL_CONTEXT_PROVIDER_H

// Null is always there
#define GL_CONTEXT_PROVIDER_NAME GLContextProviderNull
#include "GLContextProviderImpl.h"
#undef GL_CONTEXT_PROVIDER_NAME

#ifdef XP_WIN
#define GL_CONTEXT_PROVIDER_NAME GLContextProviderWGL
#include "GLContextProviderImpl.h"
#undef GL_CONTEXT_PROVIDER_NAME
#define GL_CONTEXT_PROVIDER_DEFAULT GLContextProviderWGL
#define DEFAULT_IMPL WGL
#endif

#ifdef XP_MACOSX
#define GL_CONTEXT_PROVIDER_NAME GLContextProviderCGL
#include "GLContextProviderImpl.h"
#undef GL_CONTEXT_PROVIDER_NAME
#define GL_CONTEXT_PROVIDER_DEFAULT GLContextProviderCGL
#endif

#if defined(ANDROID) || defined(XP_WIN)
#define GL_CONTEXT_PROVIDER_NAME GLContextProviderEGL
#include "GLContextProviderImpl.h"
#undef GL_CONTEXT_PROVIDER_NAME
#ifndef GL_CONTEXT_PROVIDER_DEFAULT
#define GL_CONTEXT_PROVIDER_DEFAULT GLContextProviderEGL
#endif
#endif

#ifdef MOZ_GL_PROVIDER
#define GL_CONTEXT_PROVIDER_NAME MOZ_GL_PROVIDER
#include "GLContextProviderImpl.h"
#undef GL_CONTEXT_PROVIDER_NAME
#define GL_CONTEXT_PROVIDER_DEFAULT MOZ_GL_PROVIDER
#endif

#if defined(MOZ_X11) && !defined(GL_CONTEXT_PROVIDER_DEFAULT)
#define GL_CONTEXT_PROVIDER_NAME GLContextProviderGLX
#include "GLContextProviderImpl.h"
#undef GL_CONTEXT_PROVIDER_NAME
#define GL_CONTEXT_PROVIDER_DEFAULT GLContextProviderGLX
#endif

#ifdef GL_CONTEXT_PROVIDER_DEFAULT
typedef GL_CONTEXT_PROVIDER_DEFAULT GLContextProvider;
#else
typedef GLContextProviderNull GLContextProvider;
#endif

#undef IN_GL_CONTEXT_PROVIDER_H

}
}

#endif
