/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "WebGLContext.h"

#include "nsIConsoleService.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsDOMError.h"

#include "gfxContext.h"
#include "gfxPattern.h"

#include "CanvasUtils.h"
#include "NativeJSContext.h"

#include "WebGLArray.h"

using namespace mozilla;

nsresult NS_NewCanvasRenderingContextWebGL(nsICanvasRenderingContextWebGL** aResult);

nsresult
NS_NewCanvasRenderingContextWebGL(nsICanvasRenderingContextWebGL** aResult)
{
    nsICanvasRenderingContextWebGL* ctx = new WebGLContext();
    if (!ctx)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = ctx);
    return NS_OK;
}

WebGLContext::WebGLContext()
    : gl(nsnull), mCanvasElement(nsnull), mGLPbuffer(nsnull), mWidth(0), mHeight(0),
      mInvalidated(PR_FALSE), mActiveTexture(0)
{
    mMapBuffers.Init();
    mMapTextures.Init();
    mMapPrograms.Init();
    mMapShaders.Init();
    mMapFramebuffers.Init();
    mMapRenderbuffers.Init();
}

WebGLContext::~WebGLContext()
{
}

void
WebGLContext::Invalidate()
{
    if (!mCanvasElement)
        return;

    if (mInvalidated)
        return;

    mInvalidated = true;
    mCanvasElement->InvalidateFrame();
}

//
// nsICanvasRenderingContextInternal
//

NS_IMETHODIMP
WebGLContext::SetCanvasElement(nsICanvasElement* aParentCanvas)
{
    nsresult rv;

    if (aParentCanvas == nsnull) {
        // we get this on shutdown; we should do some more cleanup here,
        // but instead we just let our destructor do it.
        return NS_OK;
    }

    if (!SafeToCreateCanvas3DContext(aParentCanvas))
        return NS_ERROR_FAILURE;

    //
    // Let's find our prefs
    //
    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool forceSoftware = PR_FALSE;

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch("webgl.", getter_AddRefs(prefBranch));
    if (NS_SUCCEEDED(rv)) {
        PRBool val;

        rv = prefBranch->GetBoolPref("software_render", &val);
        if (NS_SUCCEEDED(rv))
            forceSoftware = val;
    }

    LogMessage("Canvas 3D: creating PBuffer...");

    if (!forceSoftware) {
#if defined(WINCE)
        mGLPbuffer = new nsGLPbufferEGL();
#elif defined(XP_WIN)
        mGLPbuffer = new nsGLPbufferWGL();
#elif defined(XP_UNIX) && defined(MOZ_X11)
        mGLPbuffer = new nsGLPbufferGLX();
#elif defined(XP_MACOSX)
        mGLPbuffer = new nsGLPbufferCGL();
#else
        mGLPbuffer = nsnull;
#endif

        if (mGLPbuffer && !mGLPbuffer->Init(this))
            mGLPbuffer = nsnull;
    }

    if (!mGLPbuffer) {
        mGLPbuffer = new nsGLPbufferOSMESA();
        if (!mGLPbuffer->Init(this))
            mGLPbuffer = nsnull;
    }

    if (!mGLPbuffer)
        return NS_ERROR_FAILURE;

    gl = mGLPbuffer->GL();

    if (!ValidateGL()) {
        // XXX over here we need to destroy mGLPbuffer and create a mesa buffer

        LogMessage("Canvas 3D: Couldn't validate OpenGL implementation; is everything needed present?");
        return NS_ERROR_FAILURE;
    }

    mCanvasElement = aParentCanvas;
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::SetDimensions(PRInt32 width, PRInt32 height)
{
    if (mWidth == width && mHeight == height)
        return NS_OK;

    if (!mGLPbuffer->Resize(width, height)) {
        LogMessage("mGLPbuffer->Resize failed");
        return NS_ERROR_FAILURE;
    }

    LogMessage("Canvas 3D: ready");

    mWidth = width;
    mHeight = height;

    // Make sure that we clear this out, otherwise
    // we'll end up displaying random memory
#if 0
    int err = glGetError();
    if (err) {
        printf ("error before MakeContextCurrent! 0x%04x\n", err);
    }
#endif

    MakeContextCurrent();
    gl->fViewport(0, 0, mWidth, mHeight);
    gl->fClearColor(0, 0, 0, 0);
    gl->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);

#if 0
    err = glGetError();
    if (err) {
        printf ("error after MakeContextCurrent! 0x%04x\n", err);
    }
#endif

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::Render(gfxContext *ctx, gfxPattern::GraphicsFilter f)
{
    nsresult rv = NS_OK;

    if (mInvalidated) {
        mGLPbuffer->SwapBuffers();
        mInvalidated = PR_FALSE;
    }

    if (!mGLPbuffer)
        return NS_OK;

    // use GL Drawing if we can get a target GL context; otherwise
    // go through the fallback path.
#ifdef HAVE_GL_DRAWING
    if (mCanvasElement->GLWidgetBeginDrawing()) {
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        int bwidth = mGLPbuffer->Width();
        int bheight = mGLPbuffer->Height();

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, tex);

        CGLError err =
            CGLTexImagePBuffer(CGLGetCurrentContext(),
                               ((nsGLPbufferCGL*)mGLPbuffer)->GetCGLPbuffer(),
                               GL_BACK);
        if (err) {
            fprintf (stderr, "CGLTexImagePBuffer failed: %d\n", err);
            glDeleteTextures(1, &tex);
            return NS_OK;
        }

        glEnable(GL_TEXTURE_RECTANGLE_EXT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        //glFrustum(-halfWidth, halfWidth, halfHeight, -halfHeight, 1.0, 100000.0);
        glOrtho(0, bwidth, bheight, 0, -0.5, 10.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glBegin(GL_QUADS);

        /* Note that the texture needs a y-flip */
        glTexCoord2f(0.0, bheight);
        glVertex3f(0.0, 0.0, 0.0);

        glTexCoord2f(bwidth, bheight);
        glVertex3f(bwidth, 0.0, 0.0);

        glTexCoord2f(bwidth, 0);
        glVertex3f(bwidth, bheight, 0.0);

        glTexCoord2f(0.0, 0);
        glVertex3f(0.0, bheight, 0.0);

        glEnd();

        glDisable(GL_TEXTURE_RECTANGLE_EXT);
        glDeleteTextures(1, &tex);

        mCanvasElement->GLWidgetSwapBuffers();
        mCanvasElement->GLWidgetEndDrawing();
    } else
#endif
    {
        nsRefPtr<gfxASurface> surf = mGLPbuffer->ThebesSurface();
        if (!surf)
            return NS_OK;

        // XXX we can optimize this on win32 at least, by creating an upside-down
        // DIB.
        nsRefPtr<gfxPattern> pat = new gfxPattern(surf);
        gfxMatrix m;
        m.Translate(gfxPoint(0.0, mGLPbuffer->Height()));
        m.Scale(1.0, -1.0);
        pat->SetMatrix(m);

        // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
        // pixel alignment for this stuff!
        ctx->NewPath();
        ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
        ctx->Fill();
    }

    return rv;
}

NS_IMETHODIMP
WebGLContext::GetInputStream(const char* aMimeType,
                             const PRUnichar* aEncoderOptions,
                             nsIInputStream **aStream)
{
    return NS_ERROR_FAILURE;

    // XXX fix this
#if 0
    if (!mGLPbuffer ||
        !mGLPbuffer->ThebesSurface())
        return NS_ERROR_FAILURE;

    nsresult rv;
    const char encoderPrefix[] = "@mozilla.org/image/encoder;2?type=";
    nsAutoArrayPtr<char> conid(new (std::nothrow) char[strlen(encoderPrefix) + strlen(aMimeType) + 1]);

    if (!conid)
        return NS_ERROR_OUT_OF_MEMORY;

    strcpy(conid, encoderPrefix);
    strcat(conid, aMimeType);

    nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(conid);
    if (!encoder)
        return NS_ERROR_FAILURE;

    nsAutoArrayPtr<PRUint8> imageBuffer(new (std::nothrow) PRUint8[mWidth * mHeight * 4]);
    if (!imageBuffer)
        return NS_ERROR_OUT_OF_MEMORY;

    nsRefPtr<gfxImageSurface> imgsurf = new gfxImageSurface(imageBuffer.get(),
                                                            gfxIntSize(mWidth, mHeight),
                                                            mWidth * 4,
                                                            gfxASurface::ImageFormatARGB32);

    if (!imgsurf || imgsurf->CairoStatus())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxContext> ctx = new gfxContext(imgsurf);

    if (!ctx || ctx->HasError())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxASurface> surf = mGLPbuffer->ThebesSurface();
    nsRefPtr<gfxPattern> pat = CanvasGLThebes::CreatePattern(surf);
    gfxMatrix m;
    m.Translate(gfxPoint(0.0, mGLPbuffer->Height()));
    m.Scale(1.0, -1.0);
    pat->SetMatrix(m);

    // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
    // pixel alignment for this stuff!
    ctx->NewPath();
    ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->Fill();

    rv = encoder->InitFromData(imageBuffer.get(),
                               mWidth * mHeight * 4, mWidth, mHeight, mWidth * 4,
                               imgIEncoder::INPUT_FORMAT_HOSTARGB,
                               nsDependentString(aEncoderOptions));
    NS_ENSURE_SUCCESS(rv, rv);

    return CallQueryInterface(encoder, aStream);
#endif
}

NS_IMETHODIMP
WebGLContext::GetThebesSurface(gfxASurface **surface)
{
    if (!mGLPbuffer) {
        *surface = nsnull;
        return NS_ERROR_NOT_AVAILABLE;
    }

    *surface = mGLPbuffer->ThebesSurface();
    NS_IF_ADDREF(*surface);
    return NS_OK;
}


//
// XPCOM goop
//

NS_IMPL_ADDREF(WebGLContext)
NS_IMPL_RELEASE(WebGLContext)

NS_INTERFACE_MAP_BEGIN(WebGLContext)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextWebGL)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextWebGL)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasRenderingContextWebGL)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLBuffer)
NS_IMPL_RELEASE(WebGLBuffer)

NS_INTERFACE_MAP_BEGIN(WebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLBuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLTexture)
NS_IMPL_RELEASE(WebGLTexture)

NS_INTERFACE_MAP_BEGIN(WebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLTexture)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLProgram)
NS_IMPL_RELEASE(WebGLProgram)

NS_INTERFACE_MAP_BEGIN(WebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLProgram)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLShader)
NS_IMPL_RELEASE(WebGLShader)

NS_INTERFACE_MAP_BEGIN(WebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLShader)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLFramebuffer)
NS_IMPL_RELEASE(WebGLFramebuffer)

NS_INTERFACE_MAP_BEGIN(WebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLFramebuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLRenderbuffer)
NS_IMPL_RELEASE(WebGLRenderbuffer)

NS_INTERFACE_MAP_BEGIN(WebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLRenderbuffer)
NS_INTERFACE_MAP_END

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLTexture::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLTexture::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLBuffer::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLBuffer::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLProgram::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLProgram::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLShader::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLShader::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLFramebuffer::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLFramebuffer::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLRenderbuffer::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLRenderbuffer::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
