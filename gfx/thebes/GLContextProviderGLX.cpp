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
#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"

namespace mozilla {
namespace gl {

GLContextProvider sGLContextProvider;

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
        { NULL, { NULL } }
    };

    if (!LibrarySymbolLoader::LoadSymbols(mOGLLibrary, &symbols[0])) {
        NS_WARNING("Couldn't find required entry point in OpenGL shared library");
        return PR_FALSE;
    }

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
    CreateGLContext(Display *display, GLXDrawable drawable, GLXFBConfig cfg, PRBool pbuffer)
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
        int (*oldHandler)(Display *, XErrorEvent *) = XSetErrorHandler(&ctxErrorHandler);

        GLXContext context = sGLXLibrary.xCreateNewContext(display,
                                                           cfg,
                                                           GLX_RGBA_TYPE,
                                                           NULL,
                                                           True);

        XSync(display, False);
        XSetErrorHandler(oldHandler);

        if (!context || ctxErrorOccurred) {
            NS_WARNING("Failed to create GLXContext!");
            return nsnull;
        }

        nsRefPtr<GLContextGLX> glContext(new GLContextGLX(display, 
                                                          drawable, 
                                                          context,
                                                          pbuffer,
                                                          db));
        if (!glContext->Init()) {
            return nsnull;
        }

        return glContext.forget();
    }

    ~GLContextGLX()
    {
        if (mPBuffer) {
            sGLXLibrary.xDestroyPbuffer(mDisplay, mWindow);
        }

        sGLXLibrary.xDeleteContext(mDisplay, mContext);
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

    PRBool MakeCurrent()
    {
        Bool succeeded = sGLXLibrary.xMakeCurrent(mDisplay, mWindow, mContext);
        NS_ASSERTION(succeeded, "Failed to make GL context current!");
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
 
        case NativePBuffer:
            if (mPBuffer) {
                return (void *)mWindow;
            }
            // fall through

        default:
            return nsnull;
        }
    }

    virtual PRBool SwapBuffers()
    {
        if (mPBuffer || !mDoubleBuffered)
            return PR_FALSE;
        sGLXLibrary.xSwapBuffers(mDisplay, mWindow);
        return PR_TRUE;
    }

    void WindowDestroyed()
    {
        for (unsigned int i=0; i<textures.Length(); i++) {
            GLContext::DestroyTexture(textures.ElementAt(i));
        }
        textures.Clear();
    }

    // NB: we could set a flag upon WindowDestroyed() to dictate an
    // early-return from CreateTexture(), but then we would need the
    // same check before all GL calls, and that heads down a rabbit
    // hole.
    virtual GLuint CreateTexture()
    {
        GLuint tex = GLContext::CreateTexture();
        NS_ASSERTION(!textures.Contains(tex), "");
        textures.AppendElement(tex);
        return tex;
    }

    virtual void DestroyTexture(GLuint texture)
    {
        if (textures.Contains(texture)) {
            textures.RemoveElement(texture);
            GLContext::DestroyTexture(texture);
        }
    }

    virtual already_AddRefed<TextureImage>
    CreateBasicTextureImage(GLuint aTexture,
                            const nsIntSize& aSize,
                            TextureImage::ContentType aContentType,
                            GLContext* aContext);

private:
    GLContextGLX(Display *aDisplay, GLXDrawable aWindow, GLXContext aContext, PRBool aPBuffer = PR_FALSE, PRBool aDoubleBuffered=PR_FALSE)
        : mContext(aContext), 
          mDisplay(aDisplay), 
          mWindow(aWindow), 
          mPBuffer(aPBuffer),
          mDoubleBuffered(aDoubleBuffered) {}

    GLXContext mContext;
    Display *mDisplay;
    GLXDrawable mWindow;
    PRBool mPBuffer;
    PRBool mDoubleBuffered;
    nsTArray<GLuint> textures;
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
        return gfxPlatform::GetPlatform()->CreateOffscreenSurface(aSize, aFmt);
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

static PRBool AreCompatibleVisuals(XVisualInfo *one, XVisualInfo *two)
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
GLContextProvider::CreateForWindow(nsIWidget *aWidget)
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

    const char *vendor = sGLXLibrary.xQueryServerString(display, xscreen, GLX_VENDOR);
    PRBool isATI = vendor && strstr(vendor, "ATI");
    int numConfigs;
    ScopedXFree<GLXFBConfig> cfgs;
    if (isATI) {
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
    if (isATI) {
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
    for (int i = 0; i < numConfigs; i++) {
        ScopedXFree<XVisualInfo> info(sGLXLibrary.xGetVisualFromFBConfig(display, cfgs[i]));
        if (!info) {
            continue;
        }
        if (isATI) {
            if (AreCompatibleVisuals(vi, info)) {
                matchIndex = i;
                break;
            }
        } else {
            if (widgetVisualID == info->visualid) {
                matchIndex = i;
                break;
            }
        }
    }

    if (matchIndex == -1) {
        NS_WARNING("[GLX] Couldn't find a FBConfig matching widget visual");
        return nsnull;
    }

    nsRefPtr<GLContextGLX> glContext = GLContextGLX::CreateGLContext(display,
                                                                     window,
                                                                     cfgs[matchIndex],
                                                                     PR_FALSE);
    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProvider::CreatePBuffer(const gfxIntSize &aSize, const ContextFormat& aFormat)
{
    if (!sGLXLibrary.EnsureInitialized()) {
        return nsnull;
    }

    nsTArray<int> attribs;

#define A1_(_x)  do { attribs.AppendElement(_x); } while(0)
#define A2_(_x,_y)  do {                                                \
        attribs.AppendElement(_x);                                      \
        attribs.AppendElement(_y);                                      \
    } while(0)

    int numFormats;
    Display *display = DefaultXDisplay();
    int xscreen = DefaultScreen(display);

    A2_(GLX_DOUBLEBUFFER, False);
    A2_(GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT);

    A2_(GLX_RED_SIZE, aFormat.red);
    A2_(GLX_GREEN_SIZE, aFormat.green);
    A2_(GLX_BLUE_SIZE, aFormat.blue);
    A2_(GLX_ALPHA_SIZE, aFormat.alpha);
    A2_(GLX_DEPTH_SIZE, aFormat.depth);
    A1_(0);

    ScopedXFree<GLXFBConfig> cfg(sGLXLibrary.xChooseFBConfig(display,
                                                             xscreen,
                                                             attribs.Elements(),
                                                             &numFormats));
    if (!cfg) {
        return nsnull;
    }
    NS_ASSERTION(numFormats > 0,
                 "glXChooseFBConfig() failed to match our requested format and violated its spec (!)");
   
    nsTArray<int> pbattribs;
    pbattribs.AppendElement(GLX_PBUFFER_WIDTH);
    pbattribs.AppendElement(aSize.width);
    pbattribs.AppendElement(GLX_PBUFFER_HEIGHT);
    pbattribs.AppendElement(aSize.height);
    pbattribs.AppendElement(GLX_PRESERVED_CONTENTS);
    pbattribs.AppendElement(True);

    GLXPbuffer pbuffer = sGLXLibrary.xCreatePbuffer(display,
                                                    cfg[0],
                                                    pbattribs.Elements());

    if (pbuffer == 0) {
        return nsnull;
    }

    nsRefPtr<GLContextGLX> glContext = GLContextGLX::CreateGLContext(display,
                                                                     pbuffer,
                                                                     cfg[0],
                                                                     PR_TRUE);
    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProvider::CreateForNativePixmapSurface(gfxASurface *aSurface)
{
    return nsnull;
}

} /* namespace gl */
} /* namespace mozilla */
