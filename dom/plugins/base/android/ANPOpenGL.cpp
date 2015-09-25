/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>
#include <android/log.h>
#include "AndroidBridge.h"
#include "ANPBase.h"
#include "GLContextProvider.h"
#include "nsNPAPIPluginInstance.h"
#include "nsPluginInstanceOwner.h"
#include "GLContextProvider.h"
#include "GLContextEGL.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_opengl_##name

using namespace mozilla;
using namespace mozilla::gl;

typedef nsNPAPIPluginInstance::TextureInfo TextureInfo;

static ANPEGLContext anp_opengl_acquireContext(NPP instance) {
    nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);

    GLContext* context = pinst->GLContext();
    if (!context)
        return nullptr;

    context->MakeCurrent();
    return GLContextEGL::Cast(context)->mContext;
}

static ANPTextureInfo anp_opengl_lockTexture(NPP instance) {
    nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);

    TextureInfo pluginInfo = pinst->LockContentTexture();

    ANPTextureInfo info;
    info.textureId = pluginInfo.mTexture;
    info.width = pluginInfo.mWidth;
    info.height = pluginInfo.mHeight;

    // It looks like we should be passing whatever
    // internal format Flash told us it used previously
    // (e.g., the value of pluginInfo.mInternalFormat),
    // but if we do that it doesn't upload to the texture
    // for some reason.
    info.internalFormat = 0;

    return info;
}

static void anp_opengl_releaseTexture(NPP instance, const ANPTextureInfo* info) {
    nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);

    TextureInfo pluginInfo(info->textureId, info->width, info->height, info->internalFormat);
    pinst->ReleaseContentTexture(pluginInfo);
    pinst->RedrawPlugin();
}

static void anp_opengl_invertPluginContent(NPP instance, bool isContentInverted) {
    // OpenGL is BottomLeft if uninverted.
    gl::OriginPos newOriginPos = gl::OriginPos::BottomLeft;
    if (isContentInverted)
        newOriginPos = gl::OriginPos::TopLeft;

    nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(instance->ndata);

    pinst->SetOriginPos(newOriginPos);
    pinst->RedrawPlugin();
}

///////////////////////////////////////////////////////////////////////////////

void InitOpenGLInterface(ANPOpenGLInterfaceV0* i) {
    ASSIGN(i, acquireContext);
    ASSIGN(i, lockTexture);
    ASSIGN(i, releaseTexture);
    ASSIGN(i, invertPluginContent);
}
