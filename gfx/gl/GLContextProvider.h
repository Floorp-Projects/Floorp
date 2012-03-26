/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef GLCONTEXTPROVIDER_H_
#define GLCONTEXTPROVIDER_H_

#include "GLContext.h"
#include "gfxTypes.h"
#include "gfxPoint.h"
#include "nsAutoPtr.h"

class nsIWidget;
class gfxASurface;

namespace mozilla {
namespace gl {

#define IN_GL_CONTEXT_PROVIDER_H

// Null and OSMesa are always there
#define GL_CONTEXT_PROVIDER_NAME GLContextProviderNull
#include "GLContextProviderImpl.h"
#undef GL_CONTEXT_PROVIDER_NAME

#define GL_CONTEXT_PROVIDER_NAME GLContextProviderOSMesa
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

#if defined(ANDROID) || defined(MOZ_PLATFORM_MAEMO) || defined(XP_WIN)
#define GL_CONTEXT_PROVIDER_NAME GLContextProviderEGL
#include "GLContextProviderImpl.h"
#undef GL_CONTEXT_PROVIDER_NAME

#ifndef GL_CONTEXT_PROVIDER_DEFAULT
#define GL_CONTEXT_PROVIDER_DEFAULT GLContextProviderEGL
#endif
#endif

// X11, with XRender optimizations and no GL layer support
#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE) && !defined(GL_CONTEXT_PROVIDER_DEFAULT)
#define GL_CONTEXT_PROVIDER_NAME GLContextProviderEGL
#include "GLContextProviderImpl.h"
#undef GL_CONTEXT_PROVIDER_NAME
#define GL_CONTEXT_PROVIDER_DEFAULT GLContextProviderEGL
#endif

// X11, but only if we didn't use EGL above
#if defined(MOZ_X11) && !defined(GL_CONTEXT_PROVIDER_DEFAULT)
#define GL_CONTEXT_PROVIDER_NAME GLContextProviderGLX
#include "GLContextProviderImpl.h"
#undef GL_CONTEXT_PROVIDER_NAME
#define GL_CONTEXT_PROVIDER_DEFAULT GLContextProviderGLX
#endif

#ifdef MOZ_GL_PROVIDER
#define GL_CONTEXT_PROVIDER_NAME MOZ_GL_PROVIDER
#include "GLContextProviderImpl.h"
#undef GL_CONTEXT_PROVIDER_NAME
#define GL_CONTEXT_PROVIDER_DEFAULT MOZ_GL_PROVIDER
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
