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
 *   Matt Woodrow <mwoodrow@mozilla.com>
 *   Bas Schouten <bschouten@mozilla.com>
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

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#define GET_NATIVE_WINDOW(aWidget) GDK_WINDOW_XID((GdkWindow *) aWidget->GetNativeData(NS_NATIVE_WINDOW))
#elif defined(MOZ_WIDGET_QT)
#include <QWidget>
#include <QX11Info>
#define GET_NATIVE_WINDOW(aWidget) static_cast<QWidget*>(aWidget->GetNativeData(NS_NATIVE_SHELLWIDGET))->handle()
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "mozilla/X11Util.h"

#include "GLContextProvider.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "GLXLibrary.h"
#include "gfxXlibSurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"

namespace mozilla {
namespace gl {

static PRBool gIsATI = PR_FALSE;
static PRBool gIsChromium = PR_FALSE;
static int gGLXVersion = 0;

PRBool
GLXLibrary::EnsureInitialized()
{
    if (mInitialized) {
        return PR_TRUE;
    }

    if (!mOGLLibrary) {
        mOGLLibrary = PR_LoadLibrary("libGL.so.1");
        if (!mOGLLibrary) {
	    NS_WARNING("Couldn't load OpenGL shared library.");
	    return PR_FALSE;
        }
    }

    LibrarySymbolLoader::SymLoadStruct symbols[] = {
        { (PRFuncPtr*) &xDeleteContext, { "glXDestroyContext", NULL } },
        { (PRFuncPtr*) &xMakeCurrent, { "glXMakeCurrent", NULL } },
        { (PRFuncPtr*) &xGetProcAddress, { "glXGetProcAddress", NULL } },
        { (PRFuncPtr*) &xChooseVisual, { "glXChooseVisual", NULL } },
        { (PRFuncPtr*) &xChooseFBConfig, { "glXChooseFBConfig", NULL } },
        { (PRFuncPtr*) &xGetFBConfigs, { "glXGetFBConfigs", NULL } },
        { (PRFuncPtr*) &xCreatePbuffer, { "glXCreatePbuffer", NULL } },
        { (PRFuncPtr*) &xCreateNewContext, { "glXCreateNewContext", NULL } },
        { (PRFuncPtr*) &xDestroyPbuffer, { "glXDestroyPbuffer", NULL } },
        { (PRFuncPtr*) &xGetVisualFromFBConfig, { "glXGetVisualFromFBConfig", NULL } },
        { (PRFuncPtr*) &xGetFBConfigAttrib, { "glXGetFBConfigAttrib", NULL } },
        { (PRFuncPtr*) &xSwapBuffers, { "glXSwapBuffers", NULL } },
        { (PRFuncPtr*) &xQueryServerString, { "glXQueryServerString", NULL } },
        { (PRFuncPtr*) &xCreatePixmap, { "glXCreatePixmap", NULL } },
        { (PRFuncPtr*) &xDestroyPixmap, { "glXDestroyPixmap", NULL } },
        { (PRFuncPtr*) &xGetClientString, { "glXGetClientString", NULL } },
        { (PRFuncPtr*) &xCreateContext, { "glXCreateContext", NULL } },
        { (PRFuncPtr*) &xGetCurrentContext, { "glXGetCurrentContext", NULL } },
        { NULL, { NULL } }
    };

    if (!LibrarySymbolLoader::LoadSymbols(mOGLLibrary, &symbols[0])) {
        NS_WARNING("Couldn't find required entry point in OpenGL shared library");
        return PR_FALSE;
    }

    const char *vendor = xQueryServerString(DefaultXDisplay(),
                                            DefaultScreen(DefaultXDisplay()),
                                            GLX_VENDOR);
    const char *serverVersionStr = xQueryServerString(DefaultXDisplay(),
                                                      DefaultScreen(DefaultXDisplay()),
                                                      GLX_VERSION);
    const char *clientVersionStr = xGetClientString(DefaultXDisplay(),
                                                    GLX_VERSION);


    int serverVersion = 0, clientVersion = 0;
    if (serverVersionStr &&
        strlen(serverVersionStr) >= 3 &&
        serverVersionStr[1] == '.')
    {
        serverVersion = (serverVersionStr[0] - '0') << 8 | (serverVersionStr[2] - '0');
    }

    if (clientVersionStr &&
        strlen(clientVersionStr) >= 3 &&
        clientVersionStr[1] == '.')
    {
        clientVersion = (clientVersionStr[0] - '0') << 8 | (clientVersionStr[2] - '0');
    }

    gGLXVersion = PR_MIN(clientVersion, serverVersion);

    if (gGLXVersion < 0x0103)
        return PR_FALSE;

    gIsATI = vendor && DoesVendorStringMatch(vendor, "ATI");
    gIsChromium = (vendor && DoesVendorStringMatch(vendor, "Chromium")) ||
        (serverVersion && DoesVendorStringMatch(serverVersionStr, "Chromium"));

    mInitialized = PR_TRUE;
    return PR_TRUE;
}

GLXLibrary sGLXLibrary;

static bool ctxErrorOccurred = false;
static int
ctxErrorHandler(Display *dpy, XErrorEvent *ev)
{
    ctxErrorOccurred = true;
    return 0;
}

class GLContextGLX : public GLContext
{
public:
    static already_AddRefed<GLContextGLX>
    CreateGLContext(const ContextFormat& format,
                    Display *display,
                    GLXDrawable drawable,
                    GLXFBConfig cfg,
                    XVisualInfo *vinfo,
                    GLContextGLX *shareContext,
                    PRBool deleteDrawable,
                    gfxXlibSurface *pixmap = nsnull)
    {
        int db = 0, err;
        err = sGLXLibrary.xGetFBConfigAttrib(display, cfg,
                                             GLX_DOUBLEBUFFER, &db);
        if (GLX_BAD_ATTRIBUTE != err) {
#ifdef DEBUG
            printf("[GLX] FBConfig is %sdouble-buffered\n", db ? "" : "not ");
#endif
        }

        ctxErrorOccurred = false;
        int (*oldHandler)(Display *, XErrorEvent *);
        GLXContext context;

TRY_AGAIN_NO_SHARING:
        oldHandler = XSetErrorHandler(&ctxErrorHandler);

        if (gGLXVersion >= 0x0103) {
            context = sGLXLibrary.xCreateNewContext(display,
                                                    cfg,
                                                    GLX_RGBA_TYPE,
                                                    shareContext ? shareContext->mContext : NULL,
                                                    True);
        } else {
            context = sGLXLibrary.xCreateContext(display,
                                                 vinfo,
                                                 shareContext ? shareContext->mContext : NULL,
                                                 True);
        }

        XSync(display, False);
        XSetErrorHandler(oldHandler);

        if (!context || ctxErrorOccurred) {
            if (shareContext) {
                shareContext = nsnull;
                goto TRY_AGAIN_NO_SHARING;
            }
            NS_WARNING("Failed to create GLXContext!");
            return nsnull;
        }

        nsRefPtr<GLContextGLX> glContext(new GLContextGLX(format,
                                                          shareContext,
                                                          display, 
                                                          drawable, 
                                                          context,
                                                          deleteDrawable,
                                                          db,
                                                          pixmap));
        if (!glContext->Init()) {
            return nsnull;
        }

        return glContext.forget();
    }

    ~GLContextGLX()
    {
        MarkDestroyed();

        sGLXLibrary.xDeleteContext(mDisplay, mContext);

        if (mDeleteDrawable) {
            sGLXLibrary.xDestroyPixmap(mDisplay, mDrawable);
        }
    }

    GLContextType GetContextType() {
        return ContextTypeGLX;
    }

    PRBool Init()
    {
        MakeCurrent();
        SetupLookupFunction();
        if (!InitWithPrefix("gl", PR_TRUE)) {
            return PR_FALSE;
        }

        return IsExtensionSupported("GL_EXT_framebuffer_object");
    }

    PRBool MakeCurrent(PRBool aForce = PR_FALSE)
    {
        PRBool succeeded = PR_TRUE;

        // With the ATI FGLRX driver, glxMakeCurrent is very slow even when the context doesn't change.
        // (This is not the case with other drivers such as NVIDIA).
        // So avoid calling it more than necessary. Since GLX documentation says that:
        //     "glXGetCurrentContext returns client-side information.
        //      It does not make a round trip to the server."
        // I assume that it's not worth using our own TLS slot here.
        if (aForce || sGLXLibrary.xGetCurrentContext() != mContext) {
            succeeded = sGLXLibrary.xMakeCurrent(mDisplay, mDrawable, mContext);
            NS_ASSERTION(succeeded, "Failed to make GL context current!");
        }

        return succeeded;
    }

    PRBool SetupLookupFunction()
    {
        mLookupFunc = (PlatformLookupFunction)sGLXLibrary.xGetProcAddress;
        return PR_TRUE;
    }

    void *GetNativeData(NativeDataType aType)
    {
        switch(aType) {
        case NativeGLContext:
            return mContext;
 
        case NativeThebesSurface:
            return mPixmap;

        default:
            return nsnull;
        }
    }

    PRBool IsDoubleBuffered()
    {
        return mDoubleBuffered;
    }

    PRBool SwapBuffers()
    {
        if (!mDoubleBuffered)
            return PR_FALSE;
        sGLXLibrary.xSwapBuffers(mDisplay, mDrawable);
        return PR_TRUE;
    }

    virtual already_AddRefed<TextureImage>
    CreateBasicTextureImage(GLuint aTexture,
                            const nsIntSize& aSize,
                            TextureImage::ContentType aContentType,
                            GLContext* aContext);

private:
    friend class GLContextProviderGLX;

    GLContextGLX(const ContextFormat& aFormat,
                 GLContext *aShareContext,
                 Display *aDisplay,
                 GLXDrawable aDrawable,
                 GLXContext aContext,
                 PRBool aDeleteDrawable,
                 PRBool aDoubleBuffered,
                 gfxXlibSurface *aPixmap)
        : GLContext(aFormat, aDeleteDrawable ? PR_TRUE : PR_FALSE, aShareContext),
          mContext(aContext),
          mDisplay(aDisplay),
          mDrawable(aDrawable),
          mDeleteDrawable(aDeleteDrawable),
          mDoubleBuffered(aDoubleBuffered),
          mPixmap(aPixmap)
    { }

    GLXContext mContext;
    Display *mDisplay;
    GLXDrawable mDrawable;
    PRPackedBool mDeleteDrawable;
    PRPackedBool mDoubleBuffered;

    nsRefPtr<gfxXlibSurface> mPixmap;
};

// FIXME/bug 575505: this is a (very slow!) placeholder
// implementation.  Much better would be to create a Pixmap, wrap that
// in a GLXPixmap, and then glXBindTexImage() to our texture.
class TextureImageGLX : public BasicTextureImage
{
    friend already_AddRefed<TextureImage>
    GLContextGLX::CreateBasicTextureImage(GLuint,
                                          const nsIntSize&,
                                          TextureImage::ContentType,
                                          GLContext*);

protected:
    virtual already_AddRefed<gfxASurface>
    CreateUpdateSurface(const gfxIntSize& aSize, ImageFormat aFmt)
    {
        mUpdateFormat = aFmt;
        return gfxPlatform::GetPlatform()->CreateOffscreenSurface(aSize, gfxASurface::ContentFromFormat(aFmt));
    }

    virtual already_AddRefed<gfxImageSurface>
    GetImageForUpload(gfxASurface* aUpdateSurface)
    {
        nsRefPtr<gfxImageSurface> image =
            new gfxImageSurface(gfxIntSize(mUpdateRect.width,
                                           mUpdateRect.height),
                                mUpdateFormat);
        nsRefPtr<gfxContext> tmpContext = new gfxContext(image);

        tmpContext->SetSource(aUpdateSurface);
        tmpContext->SetOperator(gfxContext::OPERATOR_SOURCE);
        tmpContext->Paint();

        return image.forget();
    }

private:
    TextureImageGLX(GLuint aTexture,
                    const nsIntSize& aSize,
                    ContentType aContentType,
                    GLContext* aContext)
        : BasicTextureImage(aTexture, aSize, aContentType, aContext)
    {}

    ImageFormat mUpdateFormat;
};

already_AddRefed<TextureImage>
GLContextGLX::CreateBasicTextureImage(GLuint aTexture,
                                      const nsIntSize& aSize,
                                      TextureImage::ContentType aContentType,
                                      GLContext* aContext)
{
    nsRefPtr<TextureImageGLX> teximage(
        new TextureImageGLX(aTexture, aSize, aContentType, aContext));
    return teximage.forget();
}

static GLContextGLX *
GetGlobalContextGLX()
{
    return static_cast<GLContextGLX*>(GLContextProviderGLX::GetGlobalContext());
}

static PRBool
AreCompatibleVisuals(XVisualInfo *one, XVisualInfo *two)
{
    if (one->c_class != two->c_class) {
        return PR_FALSE;
    }

    if (one->depth != two->depth) {
        return PR_FALSE;
    }	

    if (one->red_mask != two->red_mask ||
        one->green_mask != two->green_mask ||
        one->blue_mask != two->blue_mask) {
        return PR_FALSE;
    }

    if (one->bits_per_rgb != two->bits_per_rgb) {
        return PR_FALSE;
    }

    return PR_TRUE;
}

already_AddRefed<GLContext>
GLContextProviderGLX::CreateForWindow(nsIWidget *aWidget)
{
    if (!sGLXLibrary.EnsureInitialized()) {
        return nsnull;
    }

    // Currently, we take whatever Visual the window already has, and
    // try to create an fbconfig for that visual.  This isn't
    // necessarily what we want in the long run; an fbconfig may not
    // be available for the existing visual, or if it is, the GL
    // performance might be suboptimal.  But using the existing visual
    // is a relatively safe intermediate step.

    Display *display = (Display*)aWidget->GetNativeData(NS_NATIVE_DISPLAY); 
    int xscreen = DefaultScreen(display);
    Window window = GET_NATIVE_WINDOW(aWidget);

    int numConfigs;
    ScopedXFree<GLXFBConfig> cfgs;
    if (gIsATI) {
        const int attribs[] = {
            GLX_DOUBLEBUFFER, False,
            0
        };
        cfgs = sGLXLibrary.xChooseFBConfig(display,
                                           xscreen,
                                           attribs,
                                           &numConfigs);
    } else {
        cfgs = sGLXLibrary.xGetFBConfigs(display,
                                         xscreen,
                                         &numConfigs);
    }

    if (!cfgs) {
        NS_WARNING("[GLX] glXGetFBConfigs() failed");
        return nsnull;
    }
    NS_ASSERTION(numConfigs > 0, "No FBConfigs found!");

    // XXX the visual ID is almost certainly the GLX_FBCONFIG_ID, so
    // we could probably do this first and replace the glXGetFBConfigs
    // with glXChooseConfigs.  Docs are sparklingly clear as always.
    XWindowAttributes widgetAttrs;
    if (!XGetWindowAttributes(display, window, &widgetAttrs)) {
        NS_WARNING("[GLX] XGetWindowAttributes() failed");
        XFree(cfgs);
        return nsnull;
    }
    const VisualID widgetVisualID = XVisualIDFromVisual(widgetAttrs.visual);
#ifdef DEBUG
    printf("[GLX] widget has VisualID 0x%lx\n", widgetVisualID);
#endif

    ScopedXFree<XVisualInfo> vi;
    if (gIsATI) {
        XVisualInfo vinfo_template;
        int nvisuals;
        vinfo_template.visual   = widgetAttrs.visual;
        vinfo_template.visualid = XVisualIDFromVisual(vinfo_template.visual);
        vinfo_template.depth    = widgetAttrs.depth;
        vinfo_template.screen   = xscreen;
        vi = XGetVisualInfo(display, VisualIDMask|VisualDepthMask|VisualScreenMask,
                            &vinfo_template, &nvisuals);
        NS_ASSERTION(vi && nvisuals == 1, "Could not locate unique matching XVisualInfo for Visual");
    }

    int matchIndex = -1;
    ScopedXFree<XVisualInfo> vinfo;

    for (int i = 0; i < numConfigs; i++) {
        vinfo = sGLXLibrary.xGetVisualFromFBConfig(display, cfgs[i]);
        if (!vinfo) {
            continue;
        }
        if (gIsATI) {
            if (AreCompatibleVisuals(vi, vinfo)) {
                matchIndex = i;
                break;
            }
        } else {
            if (widgetVisualID == vinfo->visualid) {
                matchIndex = i;
                break;
            }
        }
    }

    if (matchIndex == -1) {
        NS_WARNING("[GLX] Couldn't find a FBConfig matching widget visual");
        return nsnull;
    }

    GLContextGLX *shareContext = GetGlobalContextGLX();

    nsRefPtr<GLContextGLX> glContext = GLContextGLX::CreateGLContext(ContextFormat(ContextFormat::BasicRGB24),
                                                                     display,
                                                                     window,
                                                                     cfgs[matchIndex],
                                                                     vinfo,
                                                                     shareContext,
                                                                     PR_FALSE);
    return glContext.forget();
}

static already_AddRefed<GLContextGLX>
CreateOffscreenPixmapContext(const gfxIntSize& aSize,
                             const ContextFormat& aFormat,
                             PRBool aShare)
{
    if (!sGLXLibrary.EnsureInitialized()) {
        return nsnull;
    }

    Display *display = DefaultXDisplay();
    int xscreen = DefaultScreen(display);

    int attribs[] = {
        GLX_DOUBLEBUFFER, False,
        GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
        GLX_X_RENDERABLE, True,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_ALPHA_SIZE, 0,
        GLX_DEPTH_SIZE, 0,
        0
    };
    int numConfigs = 0;

    ScopedXFree<GLXFBConfig> cfgs;
    cfgs = sGLXLibrary.xChooseFBConfig(display,
                                       xscreen,
                                       attribs,
                                       &numConfigs);
    if (!cfgs) {
        return nsnull;
    }

    NS_ASSERTION(numConfigs > 0,
                 "glXChooseFBConfig() failed to match our requested format and violated its spec (!)");

    ScopedXFree<XVisualInfo> vinfo;
    int chosenIndex;

    for (int i = 0; i < numConfigs; ++i) {
        int dtype, visid;

        if (sGLXLibrary.xGetFBConfigAttrib(display, cfgs[i], GLX_DRAWABLE_TYPE, &dtype) != Success
            || !(dtype & GLX_PIXMAP_BIT))
        {
            continue;
        }
        if (sGLXLibrary.xGetFBConfigAttrib(display, cfgs[i], GLX_VISUAL_ID, &visid) != Success
            || visid == 0)
        {
            continue;
        }

        vinfo = sGLXLibrary.xGetVisualFromFBConfig(display, cfgs[i]);

        if (vinfo) {
            chosenIndex = i;
            break;
        }
    }

    if (!vinfo) {
        NS_WARNING("glXChooseFBConfig() didn't give us any configs with visuals!");
        return nsnull;
    }

    nsRefPtr<gfxXlibSurface> xsurface = gfxXlibSurface::Create(DefaultScreenOfDisplay(display),
                                                               vinfo->visual,
                                                               gfxIntSize(16, 16));
    if (xsurface->CairoStatus() != 0) {
        return nsnull;
    }

   
    GLXPixmap glxpixmap = sGLXLibrary.xCreatePixmap(display,
                                                    cfgs[chosenIndex],
                                                    xsurface->XDrawable(),
                                                    NULL);
    if (glxpixmap == 0) {
        return nsnull;
    }

    GLContextGLX *shareContext = aShare ? GetGlobalContextGLX() : nsnull;

    nsRefPtr<GLContextGLX> glContext = GLContextGLX::CreateGLContext(aFormat,
                                                                     display,
                                                                     glxpixmap,
                                                                     cfgs[chosenIndex],
                                                                     vinfo,
                                                                     shareContext,
                                                                     PR_TRUE,
                                                                     xsurface);

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderGLX::CreateOffscreen(const gfxIntSize& aSize,
                                      const ContextFormat& aFormat)
{

    nsRefPtr<GLContextGLX> glContext =
        CreateOffscreenPixmapContext(aSize, aFormat, PR_TRUE);

    if (!glContext) {
        return nsnull;
    }

    if (!glContext->GetSharedContext()) {
        // no point in returning anything if sharing failed, we can't
        // render from this
        return nsnull;
    }

    if (!glContext->ResizeOffscreenFBO(aSize)) {
        // we weren't able to create the initial
        // offscreen FBO, so this is dead
        return nsnull;
    }

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderGLX::CreateForNativePixmapSurface(gfxASurface *aSurface)
{
    if (!sGLXLibrary.EnsureInitialized()) {
        return nsnull;
    }

    if (aSurface->GetType() != gfxASurface::SurfaceTypeXlib) {
        NS_WARNING("GLContextProviderGLX::CreateForNativePixmapSurface called with non-Xlib surface");
        return nsnull;
    }

    nsAutoTArray<int, 20> attribs;

#define A1_(_x)  do { attribs.AppendElement(_x); } while(0)
#define A2_(_x,_y)  do {                                                \
        attribs.AppendElement(_x);                                      \
        attribs.AppendElement(_y);                                      \
    } while(0)

    A2_(GLX_DOUBLEBUFFER, False);
    A2_(GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT);
    A1_(0);

    int numFormats;
    Display *display = DefaultXDisplay();
    int xscreen = DefaultScreen(display);

    ScopedXFree<GLXFBConfig> cfg(sGLXLibrary.xChooseFBConfig(display,
                                                             xscreen,
                                                             attribs.Elements(),
                                                             &numFormats));
    if (!cfg) {
        return nsnull;
    }
    NS_ASSERTION(numFormats > 0,
                 "glXChooseFBConfig() failed to match our requested format and violated its spec (!)");

    gfxXlibSurface *xs = static_cast<gfxXlibSurface*>(aSurface);

    GLXPixmap glxpixmap = sGLXLibrary.xCreatePixmap(display,
                                                    cfg[0],
                                                    xs->XDrawable(),
                                                    NULL);

    nsRefPtr<GLContextGLX> glContext = GLContextGLX::CreateGLContext(ContextFormat(ContextFormat::BasicRGB24),
                                                                     display,
                                                                     glxpixmap,
                                                                     cfg[0],
                                                                     NULL,
                                                                     NULL,
                                                                     PR_FALSE,
                                                                     xs);

    return glContext.forget();
}

static nsRefPtr<GLContext> gGlobalContext;

GLContext *
GLContextProviderGLX::GetGlobalContext()
{
    static bool triedToCreateContext = false;
    if (!triedToCreateContext && !gGlobalContext) {
        triedToCreateContext = true;
        gGlobalContext = CreateOffscreenPixmapContext(gfxIntSize(1, 1),
                                                      ContextFormat(ContextFormat::BasicRGB24),
                                                      PR_FALSE);
        if (gGlobalContext)
            gGlobalContext->SetIsGlobalSharedContext(PR_TRUE);
    }

    return gGlobalContext;
}

void
GLContextProviderGLX::Shutdown()
{
    gGlobalContext = nsnull;
}

} /* namespace gl */
} /* namespace mozilla */
