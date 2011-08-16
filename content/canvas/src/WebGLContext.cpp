/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *   Mark Steele <mwsteele@gmail.com>
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

#include "WebGLContext.h"

#include "nsIConsoleService.h"
#include "nsServiceManagerUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsDOMError.h"
#include "nsIGfxInfo.h"

#include "nsIPropertyBag.h"
#include "nsIVariant.h"

#include "imgIEncoder.h"

#include "gfxContext.h"
#include "gfxPattern.h"
#include "gfxUtils.h"

#include "CanvasUtils.h"
#include "nsDisplayList.h"

#include "GLContextProvider.h"

#include "gfxCrashReporterUtils.h"

#include "nsSVGEffects.h"

#include "prenv.h"

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::gl;
using namespace mozilla::layers;

WebGLMemoryReporter* WebGLMemoryReporter::sUniqueInstance = nsnull;

NS_MEMORY_REPORTER_IMPLEMENT(WebGLTextureMemoryUsed,
                             "webgl-texture-memory",
                             KIND_OTHER,
                             UNITS_BYTES,
                             WebGLMemoryReporter::GetTextureMemoryUsed,
                             "Memory used by WebGL textures. The OpenGL implementation is free to store these textures in either video memory or main memory. This measurement is only a lower bound, actual memory usage may be higher for example if the storage is strided.")

NS_MEMORY_REPORTER_IMPLEMENT(WebGLTextureCount,
                             "webgl-texture-count",
                             KIND_OTHER,
                             UNITS_COUNT,
                             WebGLMemoryReporter::GetTextureCount,
                             "Number of WebGL textures.")

NS_MEMORY_REPORTER_IMPLEMENT(WebGLBufferMemoryUsed,
                             "webgl-buffer-memory",
                             KIND_OTHER,
                             UNITS_BYTES,
                             WebGLMemoryReporter::GetBufferMemoryUsed,
                             "Memory used by WebGL buffers. The OpenGL implementation is free to store these buffers in either video memory or main memory. This measurement is only a lower bound, actual memory usage may be higher for example if the storage is strided.")

NS_MEMORY_REPORTER_IMPLEMENT(WebGLBufferCacheMemoryUsed,
                             "explicit/webgl/buffer-cache-memory",
                             KIND_HEAP,
                             UNITS_BYTES,
                             WebGLMemoryReporter::GetBufferCacheMemoryUsed,
                             "Memory used by WebGL buffer caches. The WebGL implementation caches the contents of element array buffers only. This adds up with the webgl-buffer-memory value, but contrary to it, this one represents bytes on the heap, not managed by OpenGL.")

NS_MEMORY_REPORTER_IMPLEMENT(WebGLBufferCount,
                             "webgl-buffer-count",
                             KIND_OTHER,
                             UNITS_COUNT,
                             WebGLMemoryReporter::GetBufferCount,
                             "Number of WebGL buffers.")

NS_MEMORY_REPORTER_IMPLEMENT(WebGLRenderbufferMemoryUsed,
                             "webgl-renderbuffer-memory",
                             KIND_OTHER,
                             UNITS_BYTES,
                             WebGLMemoryReporter::GetRenderbufferMemoryUsed,
                             "Memory used by WebGL renderbuffers. The OpenGL implementation is free to store these renderbuffers in either video memory or main memory. This measurement is only a lower bound, actual memory usage may be higher for example if the storage is strided.")

NS_MEMORY_REPORTER_IMPLEMENT(WebGLRenderbufferCount,
                             "webgl-renderbuffer-count",
                             KIND_OTHER,
                             UNITS_COUNT,
                             WebGLMemoryReporter::GetRenderbufferCount,
                             "Number of WebGL renderbuffers.")

NS_MEMORY_REPORTER_IMPLEMENT(WebGLShaderSourcesSize,
                             "explicit/webgl/shader-sources-size",
                             KIND_HEAP,
                             UNITS_BYTES,
                             WebGLMemoryReporter::GetShaderSourcesSize,
                             "Combined size of WebGL shader ASCII sources, cached on the heap. This should always be at most a few kilobytes, or dozen kilobytes for very shader-intensive WebGL demos.")

NS_MEMORY_REPORTER_IMPLEMENT(WebGLShaderTranslationLogsSize,
                             "explicit/webgl/shader-translationlogs-size",
                             KIND_HEAP,
                             UNITS_BYTES,
                             WebGLMemoryReporter::GetShaderTranslationLogsSize,
                             "Combined size of WebGL shader ASCII translation logs, cached on the heap.")

NS_MEMORY_REPORTER_IMPLEMENT(WebGLShaderCount,
                             "webgl-shader-count",
                             KIND_OTHER,
                             UNITS_COUNT,
                             WebGLMemoryReporter::GetShaderCount,
                             "Number of WebGL shaders.")

NS_MEMORY_REPORTER_IMPLEMENT(WebGLContextCount,
                             "webgl-context-count",
                             KIND_OTHER,
                             UNITS_COUNT,
                             WebGLMemoryReporter::GetContextCount,
                             "Number of WebGL contexts.")

WebGLMemoryReporter* WebGLMemoryReporter::UniqueInstance()
{
    if (!sUniqueInstance) {
        sUniqueInstance = new WebGLMemoryReporter;
    }
    return sUniqueInstance;
}

WebGLMemoryReporter::WebGLMemoryReporter()
    : mTextureMemoryUsageReporter(new NS_MEMORY_REPORTER_NAME(WebGLTextureMemoryUsed))
    , mTextureCountReporter(new NS_MEMORY_REPORTER_NAME(WebGLTextureCount))
    , mBufferMemoryUsageReporter(new NS_MEMORY_REPORTER_NAME(WebGLBufferMemoryUsed))
    , mBufferCacheMemoryUsageReporter(new NS_MEMORY_REPORTER_NAME(WebGLBufferCacheMemoryUsed))
    , mBufferCountReporter(new NS_MEMORY_REPORTER_NAME(WebGLBufferCount))
    , mRenderbufferMemoryUsageReporter(new NS_MEMORY_REPORTER_NAME(WebGLRenderbufferMemoryUsed))
    , mRenderbufferCountReporter(new NS_MEMORY_REPORTER_NAME(WebGLRenderbufferCount))
    , mShaderSourcesSizeReporter(new NS_MEMORY_REPORTER_NAME(WebGLShaderSourcesSize))
    , mShaderTranslationLogsSizeReporter(new NS_MEMORY_REPORTER_NAME(WebGLShaderTranslationLogsSize))
    , mShaderCountReporter(new NS_MEMORY_REPORTER_NAME(WebGLShaderCount))
    , mContextCountReporter(new NS_MEMORY_REPORTER_NAME(WebGLContextCount))
{
    NS_RegisterMemoryReporter(mTextureMemoryUsageReporter);
    NS_RegisterMemoryReporter(mTextureCountReporter);
    NS_RegisterMemoryReporter(mBufferMemoryUsageReporter);
    NS_RegisterMemoryReporter(mBufferCacheMemoryUsageReporter);    
    NS_RegisterMemoryReporter(mBufferCountReporter);
    NS_RegisterMemoryReporter(mRenderbufferMemoryUsageReporter);
    NS_RegisterMemoryReporter(mRenderbufferCountReporter);
    NS_RegisterMemoryReporter(mShaderSourcesSizeReporter);
    NS_RegisterMemoryReporter(mShaderTranslationLogsSizeReporter);
    NS_RegisterMemoryReporter(mShaderCountReporter);
    NS_RegisterMemoryReporter(mContextCountReporter);
}

WebGLMemoryReporter::~WebGLMemoryReporter()
{
    NS_UnregisterMemoryReporter(mTextureMemoryUsageReporter);
    NS_UnregisterMemoryReporter(mTextureCountReporter);
    NS_UnregisterMemoryReporter(mBufferMemoryUsageReporter);
    NS_UnregisterMemoryReporter(mBufferCacheMemoryUsageReporter);
    NS_UnregisterMemoryReporter(mBufferCountReporter);
    NS_UnregisterMemoryReporter(mRenderbufferMemoryUsageReporter);
    NS_UnregisterMemoryReporter(mRenderbufferCountReporter);
    NS_UnregisterMemoryReporter(mShaderSourcesSizeReporter);
    NS_UnregisterMemoryReporter(mShaderTranslationLogsSizeReporter);
    NS_UnregisterMemoryReporter(mShaderCountReporter);
    NS_UnregisterMemoryReporter(mContextCountReporter);
}

nsresult NS_NewCanvasRenderingContextWebGL(nsIDOMWebGLRenderingContext** aResult);

nsresult
NS_NewCanvasRenderingContextWebGL(nsIDOMWebGLRenderingContext** aResult)
{
    nsIDOMWebGLRenderingContext* ctx = new WebGLContext();
    if (!ctx)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = ctx);
    return NS_OK;
}

WebGLContext::WebGLContext()
    : mCanvasElement(nsnull),
      gl(nsnull)
{
    mWidth = mHeight = 0;
    mGeneration = 0;
    mInvalidated = PR_FALSE;
    mResetLayer = PR_TRUE;
    mVerbose = PR_FALSE;
    mOptionsFrozen = PR_FALSE;

    mActiveTexture = 0;
    mWebGLError = LOCAL_GL_NO_ERROR;
    mPixelStoreFlipY = PR_FALSE;
    mPixelStorePremultiplyAlpha = PR_FALSE;
    mPixelStoreColorspaceConversion = BROWSER_DEFAULT_WEBGL;

    mShaderValidation = PR_TRUE;

    mMapBuffers.Init();
    mMapTextures.Init();
    mMapPrograms.Init();
    mMapShaders.Init();
    mMapFramebuffers.Init();
    mMapRenderbuffers.Init();

    mBlackTexturesAreInitialized = PR_FALSE;
    mFakeBlackStatus = DoNotNeedFakeBlack;

    mVertexAttrib0Vector[0] = 0;
    mVertexAttrib0Vector[1] = 0;
    mVertexAttrib0Vector[2] = 0;
    mVertexAttrib0Vector[3] = 1;
    mFakeVertexAttrib0BufferObjectVector[0] = 0;
    mFakeVertexAttrib0BufferObjectVector[1] = 0;
    mFakeVertexAttrib0BufferObjectVector[2] = 0;
    mFakeVertexAttrib0BufferObjectVector[3] = 1;
    mFakeVertexAttrib0BufferObjectSize = 0;
    mFakeVertexAttrib0BufferObject = 0;
    mFakeVertexAttrib0BufferStatus = VertexAttrib0Status::Default;

    // these are de default values, see 6.2 State tables in the OpenGL ES 2.0.25 spec
    mColorWriteMask[0] = 1;
    mColorWriteMask[1] = 1;
    mColorWriteMask[2] = 1;
    mColorWriteMask[3] = 1;
    mDepthWriteMask = 1;
    mColorClearValue[0] = 0.f;
    mColorClearValue[1] = 0.f;
    mColorClearValue[2] = 0.f;
    mColorClearValue[3] = 0.f;
    mDepthClearValue = 1.f;
    mStencilClearValue = 0;
    mStencilRefFront = 0;
    mStencilRefBack = 0;
    mStencilValueMaskFront = 0xffffffff;
    mStencilValueMaskBack  = 0xffffffff;
    mStencilWriteMaskFront = 0xffffffff;
    mStencilWriteMaskBack  = 0xffffffff;

    mScissorTestEnabled = 0;
    mDitherEnabled = 1;
    mBackbufferClearingStatus = BackbufferClearingStatus::NotClearedSinceLastPresented;
    
    // initialize some GL values: we're going to get them from the GL and use them as the sizes of arrays,
    // so in case glGetIntegerv leaves them uninitialized because of a GL bug, we would have very weird crashes.
    mGLMaxVertexAttribs = 0;
    mGLMaxTextureUnits = 0;
    mGLMaxTextureSize = 0;
    mGLMaxCubeMapTextureSize = 0;
    mGLMaxTextureImageUnits = 0;
    mGLMaxVertexTextureImageUnits = 0;
    mGLMaxVaryingVectors = 0;
    mGLMaxFragmentUniformVectors = 0;
    mGLMaxVertexUniformVectors = 0;
    
    // See OpenGL ES 2.0.25 spec, 6.2 State Tables, table 6.13
    mPixelStorePackAlignment = 4;
    mPixelStoreUnpackAlignment = 4;
    
    WebGLMemoryReporter::AddWebGLContext(this);
}

WebGLContext::~WebGLContext()
{
    DestroyResourcesAndContext();
    WebGLMemoryReporter::RemoveWebGLContext(this);
}

static PLDHashOperator
DeleteTextureFunction(const PRUint32& aKey, WebGLTexture *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Texture is still in mMapTextures, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteTextures(1, &name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

static PLDHashOperator
DeleteBufferFunction(const PRUint32& aKey, WebGLBuffer *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Buffer is still in mMapBuffers, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteBuffers(1, &name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

static PLDHashOperator
DeleteFramebufferFunction(const PRUint32& aKey, WebGLFramebuffer *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Framebuffer is still in mMapFramebuffers, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteFramebuffers(1, &name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

static PLDHashOperator
DeleteRenderbufferFunction(const PRUint32& aKey, WebGLRenderbuffer *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Renderbuffer is still in mMapRenderbuffers, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteRenderbuffers(1, &name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

static PLDHashOperator
DeleteProgramFunction(const PRUint32& aKey, WebGLProgram *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Program is still in mMapPrograms, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteProgram(name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

static PLDHashOperator
DeleteShaderFunction(const PRUint32& aKey, WebGLShader *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Shader is still in mMapShaders, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteShader(name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

void
WebGLContext::DestroyResourcesAndContext()
{
    if (!gl)
        return;

    gl->MakeCurrent();

    mMapTextures.EnumerateRead(DeleteTextureFunction, gl);
    mMapTextures.Clear();

    mMapBuffers.EnumerateRead(DeleteBufferFunction, gl);
    mMapBuffers.Clear();

    mMapPrograms.EnumerateRead(DeleteProgramFunction, gl);
    mMapPrograms.Clear();

    mMapShaders.EnumerateRead(DeleteShaderFunction, gl);
    mMapShaders.Clear();

    mMapFramebuffers.EnumerateRead(DeleteFramebufferFunction, gl);
    mMapFramebuffers.Clear();

    mMapRenderbuffers.EnumerateRead(DeleteRenderbufferFunction, gl);
    mMapRenderbuffers.Clear();

    if (mBlackTexturesAreInitialized) {
        gl->fDeleteTextures(1, &mBlackTexture2D);
        gl->fDeleteTextures(1, &mBlackTextureCubeMap);
        mBlackTexturesAreInitialized = PR_FALSE;
    }

    if (mFakeVertexAttrib0BufferObject) {
        gl->fDeleteBuffers(1, &mFakeVertexAttrib0BufferObject);
    }

    // We just got rid of everything, so the context had better
    // have been going away.
#ifdef DEBUG
    printf_stderr("--- WebGL context destroyed: %p\n", gl.get());
#endif

    gl = nsnull;
}

void
WebGLContext::Invalidate()
{
    if (mInvalidated)
        return;

    if (!mCanvasElement)
        return;

    nsSVGEffects::InvalidateDirectRenderingObservers(HTMLCanvasElement());

    mInvalidated = PR_TRUE;
    HTMLCanvasElement()->InvalidateCanvasContent(nsnull);
}

/* readonly attribute nsIDOMHTMLCanvasElement canvas; */
NS_IMETHODIMP
WebGLContext::GetCanvas(nsIDOMHTMLCanvasElement **canvas)
{
    NS_IF_ADDREF(*canvas = mCanvasElement);

    return NS_OK;
}

//
// nsICanvasRenderingContextInternal
//

NS_IMETHODIMP
WebGLContext::SetCanvasElement(nsHTMLCanvasElement* aParentCanvas)
{
    mCanvasElement = aParentCanvas;

    return NS_OK;
}

static bool
GetBoolFromPropertyBag(nsIPropertyBag *bag, const char *propName, bool *boolResult)
{
    nsCOMPtr<nsIVariant> vv;
    PRBool bv;

    nsresult rv = bag->GetProperty(NS_ConvertASCIItoUTF16(propName), getter_AddRefs(vv));
    if (NS_FAILED(rv) || !vv)
        return false;

    rv = vv->GetAsBool(&bv);
    if (NS_FAILED(rv))
        return false;

    *boolResult = bv ? true : false;
    return true;
}

NS_IMETHODIMP
WebGLContext::SetContextOptions(nsIPropertyBag *aOptions)
{
    if (!aOptions)
        return NS_OK;

    WebGLContextOptions newOpts;

    GetBoolFromPropertyBag(aOptions, "stencil", &newOpts.stencil);
    GetBoolFromPropertyBag(aOptions, "depth", &newOpts.depth);
    GetBoolFromPropertyBag(aOptions, "alpha", &newOpts.alpha);
    GetBoolFromPropertyBag(aOptions, "premultipliedAlpha", &newOpts.premultipliedAlpha);
    GetBoolFromPropertyBag(aOptions, "antialias", &newOpts.antialias);
    GetBoolFromPropertyBag(aOptions, "preserveDrawingBuffer", &newOpts.preserveDrawingBuffer);

    // enforce that if stencil is specified, we also give back depth
    newOpts.depth |= newOpts.stencil;

#if 0
    LogMessage("aaHint: %d stencil: %d depth: %d alpha: %d premult: %d preserve: %d\n",
               newOpts.antialias ? 1 : 0,
               newOpts.stencil ? 1 : 0,
               newOpts.depth ? 1 : 0,
               newOpts.alpha ? 1 : 0,
               newOpts.premultipliedAlpha ? 1 : 0,
               newOpts.preserveDrawingBuffer ? 1 : 0);
#endif

    if (mOptionsFrozen && newOpts != mOptions) {
        // Error if the options are already frozen, and the ones that were asked for
        // aren't the same as what they were originally.
        return NS_ERROR_FAILURE;
    }

    mOptions = newOpts;
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::SetDimensions(PRInt32 width, PRInt32 height)
{
    /*** early success return cases ***/
  
    if (mCanvasElement) {
        HTMLCanvasElement()->InvalidateCanvas();
    }

    if (gl && mWidth == width && mHeight == height)
        return NS_OK;

    // Zero-sized surfaces can cause problems.
    if (width == 0 || height == 0) {
        width = 1;
        height = 1;
    }

    // If we already have a gl context, then we just need to resize
    // FB0.
    if (gl &&
        gl->ResizeOffscreen(gfxIntSize(width, height)))
    {
        // everything's good, we're done here
        mWidth = width;
        mHeight = height;
        mResetLayer = PR_TRUE;
        return NS_OK;
    }

    /*** end of early success return cases ***/

    ScopedGfxFeatureReporter reporter("WebGL");

    // At this point we know that the old context is not going to survive, even though we still don't
    // know if creating the new context will succeed.
    DestroyResourcesAndContext();

    // Get some prefs for some preferred/overriden things
    NS_ENSURE_TRUE(Preferences::GetRootBranch(), NS_ERROR_FAILURE);

    PRBool forceOSMesa =
        Preferences::GetBool("webgl.force_osmesa", PR_FALSE);
    PRBool preferEGL =
        Preferences::GetBool("webgl.prefer-egl", PR_FALSE);
    PRBool preferOpenGL =
        Preferences::GetBool("webgl.prefer-native-gl", PR_FALSE);
    PRBool forceEnabled =
        Preferences::GetBool("webgl.force-enabled", PR_FALSE);
    PRBool disabled =
        Preferences::GetBool("webgl.disabled", PR_FALSE);
    PRBool verbose =
        Preferences::GetBool("webgl.verbose", PR_FALSE);

    if (disabled)
        return NS_ERROR_FAILURE;

    mVerbose = verbose;

    // We're going to create an entirely new context.  If our
    // generation is not 0 right now (that is, if this isn't the first
    // context we're creating), we may have to dispatch a context lost
    // event.

    // If incrementing the generation would cause overflow,
    // don't allow it.  Allowing this would allow us to use
    // resource handles created from older context generations.
    if (!(mGeneration+1).valid())
        return NS_ERROR_FAILURE; // exit without changing the value of mGeneration

    gl::ContextFormat format(gl::ContextFormat::BasicRGBA32);
    if (mOptions.depth) {
        format.depth = 24;
        format.minDepth = 16;
    }

    if (mOptions.stencil) {
        format.stencil = 8;
        format.minStencil = 8;
    }

    if (!mOptions.alpha) {
        // Select 565; we won't/shouldn't hit this on the desktop,
        // but let mobile know we're ok with it.
        format.red = 5;
        format.green = 6;
        format.blue = 5;

        format.alpha = 0;
        format.minAlpha = 0;
    }

    if (PR_GetEnv("MOZ_WEBGL_PREFER_EGL")) {
        preferEGL = PR_TRUE;
    }

    // Ask GfxInfo about what we should use
    PRBool useOpenGL = PR_TRUE;
    PRBool useANGLE = PR_TRUE;

    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    if (gfxInfo && !forceEnabled) {
        PRInt32 status;
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBGL_OPENGL, &status))) {
            if (status != nsIGfxInfo::FEATURE_NO_INFO) {
                useOpenGL = PR_FALSE;
            }
        }
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBGL_ANGLE, &status))) {
            if (status != nsIGfxInfo::FEATURE_NO_INFO) {
                useANGLE = PR_FALSE;
            }
        }
    }

    // allow forcing GL and not EGL/ANGLE
    if (PR_GetEnv("MOZ_WEBGL_FORCE_OPENGL")) {
        preferEGL = PR_FALSE;
        useANGLE = PR_FALSE;
        useOpenGL = PR_TRUE;
    }

    // if we're forcing osmesa, do it first
    if (forceOSMesa) {
        gl = gl::GLContextProviderOSMesa::CreateOffscreen(gfxIntSize(width, height), format);
        if (!gl || !InitAndValidateGL()) {
            LogMessage("OSMesa forced, but creating context failed -- aborting!");
            return NS_ERROR_FAILURE;
        }
        LogMessage("Using software rendering via OSMesa (THIS WILL BE SLOW)");
    }

#ifdef XP_WIN
    // if we want EGL, try it now
    if (!gl && (preferEGL || useANGLE) && !preferOpenGL) {
        gl = gl::GLContextProviderEGL::CreateOffscreen(gfxIntSize(width, height), format);
        if (gl && !InitAndValidateGL()) {
            gl = nsnull;
        }
    }

    // if it failed, then try the default provider, whatever that is
    if (!gl && useOpenGL) {
        gl = gl::GLContextProvider::CreateOffscreen(gfxIntSize(width, height), format);
        if (gl && !InitAndValidateGL()) {
            gl = nsnull;
        }
    }
#else
    // other platforms just use whatever the default is
    if (!gl && useOpenGL) {
        gl = gl::GLContextProvider::CreateOffscreen(gfxIntSize(width, height), format);
        if (gl && !InitAndValidateGL()) {
            gl = nsnull;
        }
    }
#endif

    // finally, try OSMesa
    if (!gl) {
        gl = gl::GLContextProviderOSMesa::CreateOffscreen(gfxIntSize(width, height), format);
        if (!gl || !InitAndValidateGL()) {
            gl = nsnull;
        } else {
            LogMessage("Using software rendering via OSMesa (THIS WILL BE SLOW)");
        }
    }

    if (!gl) {
        LogMessage("Can't get a usable WebGL context");
        return NS_ERROR_FAILURE;
    }

#ifdef DEBUG
    printf_stderr ("--- WebGL context created: %p\n", gl.get());
#endif

    mWidth = width;
    mHeight = height;
    mResetLayer = PR_TRUE;
    mOptionsFrozen = PR_TRUE;

    // increment the generation number
    ++mGeneration;

#if 0
    if (mGeneration > 0) {
        // XXX dispatch context lost event
    }
#endif

    MakeContextCurrent();

    // Make sure that we clear this out, otherwise
    // we'll end up displaying random memory
    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, gl->GetOffscreenFBO());
    gl->fViewport(0, 0, mWidth, mHeight);
    gl->fClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl->fClearDepth(1.0f);
    gl->fClearStencil(0);
    gl->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);

    reporter.SetSuccessful();
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::Render(gfxContext *ctx, gfxPattern::GraphicsFilter f)
{
    if (!gl)
        return NS_OK;

    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(gfxIntSize(mWidth, mHeight),
                                                         gfxASurface::ImageFormatARGB32);
    if (surf->CairoStatus() != 0)
        return NS_ERROR_FAILURE;

    gl->ReadPixelsIntoImageSurface(0, 0, mWidth, mHeight, surf);
    gfxUtils::PremultiplyImageSurface(surf);

    nsRefPtr<gfxPattern> pat = new gfxPattern(surf);
    pat->SetFilter(f);

    // Pixels from ReadPixels will be "upside down" compared to
    // what cairo wants, so draw with a y-flip and a translte to
    // flip them.
    gfxMatrix m;
    m.Translate(gfxPoint(0.0, mHeight));
    m.Scale(1.0, -1.0);
    pat->SetMatrix(m);

    ctx->NewPath();
    ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
    ctx->Fill();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetInputStream(const char* aMimeType,
                             const PRUnichar* aEncoderOptions,
                             nsIInputStream **aStream)
{
    NS_ASSERTION(gl, "GetInputStream on invalid context?");
    if (!gl)
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(gfxIntSize(mWidth, mHeight),
                                                         gfxASurface::ImageFormatARGB32);
    if (surf->CairoStatus() != 0)
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxContext> tmpcx = new gfxContext(surf);
    // Use Render() to make sure that appropriate y-flip gets applied
    nsresult rv = Render(tmpcx, gfxPattern::FILTER_NEAREST);
    if (NS_FAILED(rv))
        return rv;

    const char encoderPrefix[] = "@mozilla.org/image/encoder;2?type=";
    nsAutoArrayPtr<char> conid(new char[strlen(encoderPrefix) + strlen(aMimeType) + 1]);

    if (!conid)
        return NS_ERROR_OUT_OF_MEMORY;

    strcpy(conid, encoderPrefix);
    strcat(conid, aMimeType);

    nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(conid);
    if (!encoder)
        return NS_ERROR_FAILURE;

    rv = encoder->InitFromData(surf->Data(),
                               mWidth * mHeight * 4,
                               mWidth, mHeight,
                               surf->Stride(),
                               imgIEncoder::INPUT_FORMAT_HOSTARGB,
                               nsDependentString(aEncoderOptions));
    NS_ENSURE_SUCCESS(rv, rv);

    return CallQueryInterface(encoder, aStream);
}

NS_IMETHODIMP
WebGLContext::GetThebesSurface(gfxASurface **surface)
{
    return NS_ERROR_NOT_AVAILABLE;
}

static PRUint8 gWebGLLayerUserData;

class WebGLContextUserData : public LayerUserData {
public:
    WebGLContextUserData(nsHTMLCanvasElement *aContent)
    : mContent(aContent) {}
  static void DidTransactionCallback(void* aData)
  {
    static_cast<WebGLContextUserData*>(aData)->mContent->MarkContextClean();
  }

private:
  nsRefPtr<nsHTMLCanvasElement> mContent;
};

already_AddRefed<layers::CanvasLayer>
WebGLContext::GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                             CanvasLayer *aOldLayer,
                             LayerManager *aManager)
{
    if (!mResetLayer && aOldLayer &&
        aOldLayer->HasUserData(&gWebGLLayerUserData)) {
        NS_ADDREF(aOldLayer);
        return aOldLayer;
    }

    nsRefPtr<CanvasLayer> canvasLayer = aManager->CreateCanvasLayer();
    if (!canvasLayer) {
        NS_WARNING("CreateCanvasLayer returned null!");
        return nsnull;
    }
    WebGLContextUserData *userData = nsnull;
    if (aBuilder->IsPaintingToWindow()) {
      // Make the layer tell us whenever a transaction finishes (including
      // the current transaction), so we can clear our invalidation state and
      // start invalidating again. We need to do this for the layer that is
      // being painted to a window (there shouldn't be more than one at a time,
      // and if there is, flushing the invalidation state more often than
      // necessary is harmless).

      // The layer will be destroyed when we tear down the presentation
      // (at the latest), at which time this userData will be destroyed,
      // releasing the reference to the element.
      // The userData will receive DidTransactionCallbacks, which flush the
      // the invalidation state to indicate that the canvas is up to date.
      userData = new WebGLContextUserData(HTMLCanvasElement());
      canvasLayer->SetDidTransactionCallback(
              WebGLContextUserData::DidTransactionCallback, userData);
    }
    canvasLayer->SetUserData(&gWebGLLayerUserData, userData);

    CanvasLayer::Data data;

    // the gl context may either provide a native PBuffer, in which case we want to initialize
    // data with the gl context directly, or may provide a surface to which it renders (this is the case
    // of OSMesa contexts), in which case we want to initialize data with that surface.

    void* native_surface = gl->GetNativeData(gl::GLContext::NativeImageSurface);

    if (native_surface) {
        data.mSurface = static_cast<gfxASurface*>(native_surface);
    } else {
        data.mGLContext = gl.get();
    }

    data.mSize = nsIntSize(mWidth, mHeight);
    data.mGLBufferIsPremultiplied = mOptions.premultipliedAlpha ? PR_TRUE : PR_FALSE;

    canvasLayer->Initialize(data);
    PRUint32 flags = gl->CreationFormat().alpha == 0 ? Layer::CONTENT_OPAQUE : 0;
    canvasLayer->SetContentFlags(flags);
    canvasLayer->Updated();

    mResetLayer = PR_FALSE;

    mBackbufferClearingStatus = BackbufferClearingStatus::NotClearedSinceLastPresented;

    return canvasLayer.forget().get();
}

NS_IMETHODIMP
WebGLContext::GetContextAttributes(jsval *aResult)
{
    JSContext *cx = nsContentUtils::GetCurrentJSContext();
    if (!cx)
        return NS_ERROR_FAILURE;

    JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);
    if (!obj)
        return NS_ERROR_FAILURE;

    *aResult = OBJECT_TO_JSVAL(obj);

    gl::ContextFormat cf = gl->ActualFormat();

    if (!JS_DefineProperty(cx, obj, "alpha", cf.alpha > 0 ? JSVAL_TRUE : JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, obj, "depth", cf.depth > 0 ? JSVAL_TRUE : JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, obj, "stencil", cf.stencil > 0 ? JSVAL_TRUE : JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, obj, "antialias", JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, obj, "premultipliedAlpha",
                           mOptions.premultipliedAlpha ? JSVAL_TRUE : JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, obj, "preserveDrawingBuffer",
                           mOptions.preserveDrawingBuffer ? JSVAL_TRUE : JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE))
    {
        *aResult = JSVAL_VOID;
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/* [noscript] DOMString mozGetUnderlyingParamString(in WebGLenum pname); */
NS_IMETHODIMP
WebGLContext::MozGetUnderlyingParamString(PRUint32 pname, nsAString& retval)
{
    retval.SetIsVoid(PR_TRUE);

    MakeContextCurrent();

    switch (pname) {
    case LOCAL_GL_VENDOR:
    case LOCAL_GL_RENDERER:
    case LOCAL_GL_VERSION:
    case LOCAL_GL_SHADING_LANGUAGE_VERSION:
    case LOCAL_GL_EXTENSIONS: {
        const char *s = (const char *) gl->fGetString(pname);
        retval.Assign(NS_ConvertASCIItoUTF16(nsDependentCString(s)));
    }
        break;

    default:
        return NS_ERROR_INVALID_ARG;
    }

    return NS_OK;
}

bool WebGLContext::IsExtensionSupported(WebGLExtensionID ei)
{
    if (ei == WebGL_OES_texture_float) {
        MakeContextCurrent();
        return gl->IsExtensionSupported(gl->IsGLES2() ? GLContext::OES_texture_float
                                                      : GLContext::ARB_texture_float);
    }
    return false;
}

NS_IMETHODIMP
WebGLContext::GetExtension(const nsAString& aName, nsIWebGLExtension **retval)
{
    *retval = nsnull;

    // handle simple extensions that don't need custom objects first
    WebGLExtensionID ei = WebGLExtensionID_Max;
    if (aName.EqualsLiteral("OES_texture_float")) {
        if (IsExtensionSupported(WebGL_OES_texture_float))
            ei = WebGL_OES_texture_float;
    }

    // create a WebGLExtension object for extensions that don't
    // have any additional tokens or methods
    if (ei != WebGLExtensionID_Max) {
        if (!IsExtensionEnabled(ei)) {
            mEnabledExtensions[ei] = new WebGLExtension(this);
        }
        NS_ADDREF(*retval = mEnabledExtensions[ei]);
    }

    return NS_OK;
}

void
WebGLContext::ForceClearFramebufferWithDefaultValues(PRUint32 mask, const nsIntRect& viewportRect)
{
    MakeContextCurrent();

    PRBool initializeColorBuffer = 0 != (mask & LOCAL_GL_COLOR_BUFFER_BIT);
    PRBool initializeDepthBuffer = 0 != (mask & LOCAL_GL_DEPTH_BUFFER_BIT);
    PRBool initializeStencilBuffer = 0 != (mask & LOCAL_GL_STENCIL_BUFFER_BIT);

    // prepare GL state for clearing
    gl->fDisable(LOCAL_GL_SCISSOR_TEST);
    gl->fDisable(LOCAL_GL_DITHER);
    gl->PushViewportRect(viewportRect);

    if (initializeColorBuffer) {
        gl->fColorMask(1, 1, 1, 1);
        gl->fClearColor(0.f, 0.f, 0.f, 0.f);
    }

    if (initializeDepthBuffer) {
        gl->fDepthMask(1);
        gl->fClearDepth(1.0f);
    }

    if (initializeStencilBuffer) {
        gl->fStencilMask(0xffffffff);
        gl->fClearStencil(0);
    }

    // do clear
    gl->fClear(mask);

    // restore GL state after clearing
    if (initializeColorBuffer) {
        gl->fColorMask(mColorWriteMask[0],
                       mColorWriteMask[1],
                       mColorWriteMask[2],
                       mColorWriteMask[3]);
        gl->fClearColor(mColorClearValue[0],
                        mColorClearValue[1],
                        mColorClearValue[2],
                        mColorClearValue[3]);
    }

    if (initializeDepthBuffer) {
        gl->fDepthMask(mDepthWriteMask);
        gl->fClearDepth(mDepthClearValue);
    }

    if (initializeStencilBuffer) {
        gl->fStencilMaskSeparate(LOCAL_GL_FRONT, mStencilWriteMaskFront);
        gl->fStencilMaskSeparate(LOCAL_GL_BACK, mStencilWriteMaskBack);
        gl->fClearStencil(mStencilClearValue);
    }

    gl->PopViewportRect();

    if (mDitherEnabled)
        gl->fEnable(LOCAL_GL_DITHER);
    else
        gl->fDisable(LOCAL_GL_DITHER);

    if (mScissorTestEnabled)
        gl->fEnable(LOCAL_GL_SCISSOR_TEST);
    else
        gl->fDisable(LOCAL_GL_SCISSOR_TEST);
}

void
WebGLContext::EnsureBackbufferClearedAsNeeded()
{
    if (mOptions.preserveDrawingBuffer)
        return;

    NS_ABORT_IF_FALSE(!mBoundFramebuffer,
                      "EnsureBackbufferClearedAsNeeded must not be called when a FBO is bound");

    if (mBackbufferClearingStatus != BackbufferClearingStatus::NotClearedSinceLastPresented)
        return;

    mBackbufferClearingStatus = BackbufferClearingStatus::ClearedToDefaultValues;

    ForceClearFramebufferWithDefaultValues(LOCAL_GL_COLOR_BUFFER_BIT |
                                           LOCAL_GL_DEPTH_BUFFER_BIT |
                                           LOCAL_GL_STENCIL_BUFFER_BIT,
                                           nsIntRect(0, 0, mWidth, mHeight));

    Invalidate();
}

//
// XPCOM goop
//

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLContext)

NS_IMPL_CYCLE_COLLECTION_CLASS(WebGLContext)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WebGLContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCanvasElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WebGLContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCanvasElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(WebGLRenderingContext, WebGLContext)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLContext)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWebGLRenderingContext)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMWebGLRenderingContext)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLRenderingContext)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLBuffer)
NS_IMPL_RELEASE(WebGLBuffer)

DOMCI_DATA(WebGLBuffer, WebGLBuffer)

NS_INTERFACE_MAP_BEGIN(WebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(WebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLBuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLTexture)
NS_IMPL_RELEASE(WebGLTexture)

DOMCI_DATA(WebGLTexture, WebGLTexture)

NS_INTERFACE_MAP_BEGIN(WebGLTexture)
  NS_INTERFACE_MAP_ENTRY(WebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLTexture)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLProgram)
NS_IMPL_RELEASE(WebGLProgram)

DOMCI_DATA(WebGLProgram, WebGLProgram)

NS_INTERFACE_MAP_BEGIN(WebGLProgram)
  NS_INTERFACE_MAP_ENTRY(WebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLProgram)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLShader)
NS_IMPL_RELEASE(WebGLShader)

DOMCI_DATA(WebGLShader, WebGLShader)

NS_INTERFACE_MAP_BEGIN(WebGLShader)
  NS_INTERFACE_MAP_ENTRY(WebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLShader)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLFramebuffer)
NS_IMPL_RELEASE(WebGLFramebuffer)

DOMCI_DATA(WebGLFramebuffer, WebGLFramebuffer)

NS_INTERFACE_MAP_BEGIN(WebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(WebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLFramebuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLRenderbuffer)
NS_IMPL_RELEASE(WebGLRenderbuffer)

DOMCI_DATA(WebGLRenderbuffer, WebGLRenderbuffer)

NS_INTERFACE_MAP_BEGIN(WebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(WebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLRenderbuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLUniformLocation)
NS_IMPL_RELEASE(WebGLUniformLocation)

DOMCI_DATA(WebGLUniformLocation, WebGLUniformLocation)

NS_INTERFACE_MAP_BEGIN(WebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(WebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLUniformLocation)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLActiveInfo)
NS_IMPL_RELEASE(WebGLActiveInfo)

DOMCI_DATA(WebGLActiveInfo, WebGLActiveInfo)

NS_INTERFACE_MAP_BEGIN(WebGLActiveInfo)
  NS_INTERFACE_MAP_ENTRY(WebGLActiveInfo)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLActiveInfo)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLActiveInfo)
NS_INTERFACE_MAP_END

#define NAME_NOT_SUPPORTED(base) \
NS_IMETHODIMP base::GetName(WebGLuint *aName) \
{ return NS_ERROR_NOT_IMPLEMENTED; } \
NS_IMETHODIMP base::SetName(WebGLuint aName) \
{ return NS_ERROR_NOT_IMPLEMENTED; }

NAME_NOT_SUPPORTED(WebGLTexture)
NAME_NOT_SUPPORTED(WebGLBuffer)
NAME_NOT_SUPPORTED(WebGLProgram)
NAME_NOT_SUPPORTED(WebGLShader)
NAME_NOT_SUPPORTED(WebGLFramebuffer)
NAME_NOT_SUPPORTED(WebGLRenderbuffer)

NS_IMPL_ADDREF(WebGLExtension)
NS_IMPL_RELEASE(WebGLExtension)

DOMCI_DATA(WebGLExtension, WebGLExtension)

NS_INTERFACE_MAP_BEGIN(WebGLExtension)
  NS_INTERFACE_MAP_ENTRY(WebGLExtension)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLExtension)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLExtension)
NS_INTERFACE_MAP_END

/* [noscript] attribute WebGLint location; */
NS_IMETHODIMP
WebGLUniformLocation::GetLocation(WebGLint *aLocation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebGLUniformLocation::SetLocation(WebGLint aLocation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute WebGLint size; */
NS_IMETHODIMP
WebGLActiveInfo::GetSize(WebGLint *aSize)
{
    *aSize = mSize;
    return NS_OK;
}

/* readonly attribute WebGLenum type; */
NS_IMETHODIMP
WebGLActiveInfo::GetType(WebGLenum *aType)
{
    *aType = mType;
    return NS_OK;
}

/* readonly attribute DOMString name; */
NS_IMETHODIMP
WebGLActiveInfo::GetName(nsAString & aName)
{
    aName = mName;
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetSupportedExtensions(nsIVariant **retval)
{
    nsCOMPtr<nsIWritableVariant> wrval = do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_TRUE(wrval, NS_ERROR_FAILURE);

    nsTArray<const char *> extList;

    if (IsExtensionSupported(WebGL_OES_texture_float))
        extList.InsertElementAt(extList.Length(), "OES_texture_float");

    nsresult rv;
    if (extList.Length() > 0) {
        rv = wrval->SetAsArray(nsIDataType::VTYPE_CHAR_STR, nsnull,
                               extList.Length(), &extList[0]);
    } else {
        rv = wrval->SetAsEmptyArray();
    }
    if (NS_FAILED(rv))
        return rv;

    *retval = wrval.forget().get();
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsContextLost(WebGLboolean *retval)
{
    *retval = PR_FALSE;
    return NS_OK;
}

