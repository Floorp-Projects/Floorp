/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Mark Steele <mwsteele@gmail.com>
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

#ifndef GLCONTEXT_H_
#define GLCONTEXT_H_

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "GLDefs.h"
#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxRect.h"
#include "nsISupportsImpl.h"
#include "prlink.h"

#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsRegion.h"
#include "nsAutoPtr.h"

#ifndef GLAPIENTRY
#ifdef XP_WIN
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif
#define GLAPI
#endif

#if defined(MOZ_PLATFORM_MAEMO) || defined(ANDROID)
#define USE_GLES2 1
#endif

typedef char realGLboolean;

namespace mozilla {
namespace gl {

class GLContext;

class LibrarySymbolLoader
{
public:
    PRBool OpenLibrary(const char *library);

    typedef PRFuncPtr (GLAPIENTRY * PlatformLookupFunction) (const char *);

    enum {
        MAX_SYMBOL_NAMES = 5,
        MAX_SYMBOL_LENGTH = 128
    };

    typedef struct {
        PRFuncPtr *symPointer;
        const char *symNames[MAX_SYMBOL_NAMES];
    } SymLoadStruct;

    PRBool LoadSymbols(SymLoadStruct *firstStruct,
                       PRBool tryplatform = PR_FALSE,
                       const char *prefix = nsnull);

    /*
     * Static version of the functions in this class
     */
    static PRFuncPtr LookupSymbol(PRLibrary *lib,
                                  const char *symname,
                                  PlatformLookupFunction lookupFunction = nsnull);
    static PRBool LoadSymbols(PRLibrary *lib,
                              SymLoadStruct *firstStruct,
                              PlatformLookupFunction lookupFunction = nsnull,
                              const char *prefix = nsnull);
protected:
    LibrarySymbolLoader() {
        mLibrary = nsnull;
        mLookupFunc = nsnull;
    }

    PRLibrary *mLibrary;
    PlatformLookupFunction mLookupFunc;
};


/**
 * A TextureImage encapsulates a surface that can be drawn to by a
 * Thebes gfxContext and (hopefully efficiently!) synchronized to a
 * texture in the server.  TextureImages are associated with one and
 * only one GLContext.
 *
 * Implementation note: TextureImages attempt to unify two categories
 * of backends
 *
 *  (1) proxy to server-side object that can be bound to a texture;
 *      e.g. Pixmap on X11.
 *
 *  (2) efficient manager of texture memory; e.g. by having clients draw
 *      into a scratch buffer which is then uploaded with
 *      glTexSubImage2D().
 */
class TextureImage
{
    NS_INLINE_DECL_REFCOUNTING(TextureImage)
public:
    typedef gfxASurface::gfxContentType ContentType;

    virtual ~TextureImage() {}

    /**
     * Return a gfxContext for updating |aRegion| of the client's
     * image if successul, NULL if not.  |aRegion|'s bounds must fit
     * within Size(); its coordinate space (if any) is ignored.  If
     * the update begins successfully, the returned gfxContext is
     * owned by this.  Otherwise, NULL is returned.
     *
     * |aRegion| is an inout param: the returned region is what the
     * client must repaint.  Category (1) regions above can
     * efficiently handle repaints to "scattered" regions, while (2)
     * can only efficiently handle repaints to rects.
     *
     * The returned context is neither translated nor clipped: it's a
     * context for rect(<0,0>, Size()).  Painting the returned context
     * outside of |aRegion| results in undefined behavior.
     *
     * BeginUpdate() calls cannot be "nested", and each successful
     * BeginUpdate() must be followed by exactly one EndUpdate() (see
     * below).  Failure to do so can leave this in a possibly
     * inconsistent state.  Unsuccessful BeginUpdate()s must not be
     * followed by EndUpdate().
     */
    virtual gfxContext* BeginUpdate(nsIntRegion& aRegion) = 0;
    /**
     * Finish the active update and synchronize with the server, if
     * necessary.  Return PR_TRUE iff this's texture is already bound.
     *
     * BeginUpdate() must have been called exactly once before
     * EndUpdate().
     */
    virtual PRBool EndUpdate() = 0;

    /**
     * Set this TextureImage's size, and ensure a texture has been
     * allocated.  Must not be called between BeginUpdate and EndUpdate.
     * After a resize, the contents are undefined.
     *
     * If this isn't implemented by a subclass, it will just perform
     * a dummy BeginUpdate/EndUpdate pair.
     */
    virtual void Resize(const nsIntSize& aSize) {
        nsIntRegion r(nsIntRect(0, 0, aSize.width, aSize.height));
        gfxContext *dummy = BeginUpdate(r);
        EndUpdate();
    }

    /**
     * Return this TextureImage's texture ID for use with GL APIs.
     * Callers are responsible for properly binding the texture etc.
     *
     * The texture is only texture complete after either Resize
     * or a matching pair of BeginUpdate/EndUpdate have been called.
     * Otherwise, a texture ID may be returned, but the texture
     * may not be texture complete.
     */
    GLuint Texture() { return mTexture; }

    /** Can be called safely at any time. */

    /**
     * If this TextureImage has a permanent gfxASurface backing,
     * return it.  Otherwise return NULL.
     */
    virtual already_AddRefed<gfxASurface> GetBackingSurface()
    { return NULL; }

    const nsIntSize& GetSize() const { return mSize; }
    ContentType GetContentType() const { return mContentType; }
    virtual PRBool InUpdate() const = 0;

    PRBool IsRGB() const { return mIsRGBFormat; }

protected:
    friend class GLContext;

    /**
     * After the ctor, the TextureImage is invalid.  Implementations
     * must allocate resources successfully before returning the new
     * TextureImage from GLContext::CreateTextureImage().  That is,
     * clients must not be given partially-constructed TextureImages.
     */
    TextureImage(GLuint aTexture, const nsIntSize& aSize, ContentType aContentType, PRBool aIsRGB = PR_FALSE)
        : mTexture(aTexture)
        , mSize(aSize)
        , mContentType(aContentType)
        , mIsRGBFormat(aIsRGB)
    {}

    GLuint mTexture;
    nsIntSize mSize;
    ContentType mContentType;
    PRPackedBool mIsRGBFormat;
};

/**
 * BasicTextureImage is the baseline TextureImage implementation ---
 * it updates its texture by allocating a scratch buffer for the
 * client to draw into, then using glTexSubImage2D() to upload the new
 * pixels.  Platforms must provide the code to create a new surface
 * into which the updated pixels will be drawn, and the code to
 * convert the update surface's pixels into an image on which we can
 * glTexSubImage2D().
 */
class BasicTextureImage
    : public TextureImage
{
public:
    virtual ~BasicTextureImage();

    virtual gfxContext* BeginUpdate(nsIntRegion& aRegion);
    virtual PRBool EndUpdate();

    virtual PRBool InUpdate() const { return !!mUpdateContext; }

    virtual void Resize(const nsIntSize& aSize);
protected:
    typedef gfxASurface::gfxImageFormat ImageFormat;

    BasicTextureImage(GLuint aTexture,
                      const nsIntSize& aSize,
                      ContentType aContentType,
                      GLContext* aContext)
        : TextureImage(aTexture, aSize, aContentType)
        , mTextureInited(PR_FALSE)
        , mGLContext(aContext)
    {}

    virtual already_AddRefed<gfxASurface>
    CreateUpdateSurface(const gfxIntSize& aSize, ImageFormat aFmt) = 0;

    virtual already_AddRefed<gfxImageSurface>
    GetImageForUpload(gfxASurface* aUpdateSurface) = 0;

    PRBool mTextureInited;
    GLContext* mGLContext;
    nsRefPtr<gfxContext> mUpdateContext;
    nsIntRect mUpdateRect;
};

struct THEBES_API ContextFormat {
    static const ContextFormat BasicRGBA32Format;

    enum StandardContextFormat {
        Empty,
        BasicRGBA32,
        StrictBasicRGBA32,
        BasicRGB24,
        StrictBasicRGB24,
        BasicRGB16_565,
        StrictBasicRGB16_565
    };

    ContextFormat() {
        memset(this, 0, sizeof(ContextFormat));
    }

    ContextFormat(const StandardContextFormat cf) {
        memset(this, 0, sizeof(ContextFormat));

        switch (cf) {
        case BasicRGBA32:
            red = green = blue = alpha = 8;
            minRed = minGreen = minBlue = minAlpha = 1;
            break;

        case StrictBasicRGBA32:
            red = green = blue = alpha = 8;
            minRed = minGreen = minBlue = minAlpha = 8;
            break;

        case BasicRGB24:
            red = green = blue = 8;
            minRed = minGreen = minBlue = 1;
            break;

        case StrictBasicRGB24:
            red = green = blue = 8;
            minRed = minGreen = minBlue = 8;
            break;

        case StrictBasicRGB16_565:
            red = minRed = 5;
            green = minGreen = 6;
            blue = minBlue = 5;
            break;

        default:
            break;
        }
    }

    int depth, minDepth;
    int stencil, minStencil;
    int red, minRed;
    int green, minGreen;
    int blue, minBlue;
    int alpha, minAlpha;

    int colorBits() const { return red + green + blue; }
};

class GLContext
    : public LibrarySymbolLoader
{
    THEBES_INLINE_DECL_THREADSAFE_REFCOUNTING(GLContext)
public:
    GLContext(const ContextFormat& aFormat,
              PRBool aIsOffscreen = PR_FALSE,
              GLContext *aSharedContext = nsnull)
      : mInitialized(PR_FALSE),
        mIsOffscreen(aIsOffscreen),
#ifdef USE_GLES2
        mIsGLES2(PR_TRUE),
#else
        mIsGLES2(PR_FALSE),
#endif
        mIsGlobalSharedContext(PR_FALSE),
        mWindowOriginBottomLeft(PR_FALSE),
        mVendor(-1),
        mCreationFormat(aFormat),
        mSharedContext(aSharedContext),
        mOffscreenTexture(0),
        mBlitProgram(0),
        mBlitFramebuffer(0),
        mOffscreenFBO(0),
        mOffscreenDepthRB(0),
        mOffscreenStencilRB(0)
    {
        mUserData.Init();
    }

    virtual ~GLContext() {
        NS_ASSERTION(IsDestroyed(), "GLContext implementation must call MarkDestroyed in destructor!");
#ifdef DEBUG
        if (mSharedContext) {
            GLContext *tip = mSharedContext;
            while (tip->mSharedContext)
                tip = tip->mSharedContext;
            tip->SharedContextDestroyed(this);
            tip->ReportOutstandingNames();
        }
#endif
    }

    enum GLContextType {
        ContextTypeUnknown,
        ContextTypeWGL,
        ContextTypeCGL,
        ContextTypeGLX,
        ContextTypeEGL,
        ContextTypeOSMesa
    };

    virtual GLContextType GetContextType() { return ContextTypeUnknown; }
    virtual PRBool MakeCurrent(PRBool aForce = PR_FALSE) = 0;
    virtual PRBool SetupLookupFunction() = 0;

    virtual void WindowDestroyed() {}

    void *GetUserData(void *aKey) {
        void *result = nsnull;
        mUserData.Get(aKey, &result);
        return result;
    }

    void SetUserData(void *aKey, void *aValue) {
        mUserData.Put(aKey, aValue);
    }

    // Mark this context as destroyed.  This will NULL out all
    // the GL function pointers!
    void THEBES_API MarkDestroyed();

    PRBool IsDestroyed() {
        // MarkDestroyed will mark all these as null.
        return fUseProgram == nsnull;
    }

    enum NativeDataType {
      NativeGLContext,
      NativeImageSurface,
      NativeThebesSurface,
      NativeDataTypeMax
    };

    virtual void *GetNativeData(NativeDataType aType) { return NULL; }
    GLContext *GetSharedContext() { return mSharedContext; }

    PRBool IsGlobalSharedContext() { return mIsGlobalSharedContext; }
    void SetIsGlobalSharedContext(PRBool aIsOne) { mIsGlobalSharedContext = aIsOne; }

    const ContextFormat& CreationFormat() { return mCreationFormat; }
    const ContextFormat& ActualFormat() { return mActualFormat; }

    /**
     * If this context is double-buffered, returns TRUE.
     */
    virtual PRBool IsDoubleBuffered() { return PR_FALSE; }

    /**
     * If this context is the GLES2 API, returns TRUE.
     * This means that various GLES2 restrictions might be in effect (modulo
     * extensions).
     */
    PRBool IsGLES2() {
        return mIsGLES2;
    }

    enum { VendorIntel, VendorNVIDIA, VendorATI, VendorOther };

    PRBool Vendor() const {
        return mVendor;
    }

    /**
     * Returns PR_TRUE if the window coordinate origin is the bottom
     * left corener.  If PR_FALSE, it is the top left corner.
     *
     * This needs to be taken into account when calling glViewport
     * and glScissor when drawing directly to a window.  If this is
     * PR_FALSE, the y coordinate given to those functions should be
     * (windowHeight - (desiredHeight + desiredY)).
     *
     * This should only be done when drawing directly to a window;
     * when drawing to a FBO, the origin is always the bottom left.
     *
     * See FixWindowCoordinateRect().
     */
    PRBool IsWindowOriginBottomLeft() {
        return mWindowOriginBottomLeft;
    }

    /**
     * Fix up the rectangle given in aRect, taking into account
     * window height aWindowHeight and whether windows have their
     * natural origin in the bottom left or not.
     */
    nsIntRect& FixWindowCoordinateRect(nsIntRect& aRect, int aWindowHeight) {
        if (!mWindowOriginBottomLeft) {
            aRect.y = aWindowHeight - (aRect.height + aRect.y);
        }
        return aRect;
    }

    /**
     * If this context wraps a double-buffered target, swap the back
     * and front buffers.  It should be assumed that after a swap, the
     * contents of the new back buffer are undefined.
     */
    virtual PRBool SwapBuffers() { return PR_FALSE; }

    /**
     * Defines a two-dimensional texture image for context target surface
     */
    virtual PRBool BindTexImage() { return PR_FALSE; }
    /*
     * Releases a color buffer that is being used as a texture
     */
    virtual PRBool ReleaseTexImage() { return PR_FALSE; }

    /*
     * Offscreen support API
     */

    /*
     * Bind aOffscreen's color buffer as a texture to the TEXTURE_2D
     * target.  Returns TRUE on success, otherwise FALSE.  If
     * aOffscreen is not an offscreen context, returns FALSE.  If
     * BindOffscreenNeedsTexture() returns TRUE, then you should have
     * a 2D texture name bound whose image will be replaced by the
     * contents of the offscreen context.  If it returns FALSE,
     * the current 2D texture binding will be replaced.
     *
     * After a successul call to BindTex2DOffscreen, UnbindTex2DOffscreen
     * *must* be called once rendering is complete.
     *
     * The same texture unit must be active for Bind/Unbind of a given
     * context.
     */
    virtual PRBool BindOffscreenNeedsTexture(GLContext *aOffscreen) {
        return aOffscreen->mOffscreenTexture == 0;
    }

    virtual PRBool BindTex2DOffscreen(GLContext *aOffscreen) {
        if (aOffscreen->GetContextType() != GetContextType()) {
          return PR_FALSE;
        }

        if (!aOffscreen->mOffscreenFBO) {
            return PR_FALSE;
        }

        if (!aOffscreen->mSharedContext ||
            aOffscreen->mSharedContext != mSharedContext)
        {
            return PR_FALSE;
        }

        fBindTexture(LOCAL_GL_TEXTURE_2D, aOffscreen->mOffscreenTexture);

        return PR_TRUE;
    }

    virtual void UnbindTex2DOffscreen(GLContext *aOffscreen) { }

    PRBool IsOffscreen() {
        return mIsOffscreen;
    }

    /*
     * Resize the current offscreen buffer.  Returns true on success.
     * If it returns false, the context should be treated as unusable
     * and should be recreated.  After the resize, the viewport is not
     * changed; glViewport should be called as appropriate.
     *
     * Only valid if IsOffscreen() returns true.
     */
    virtual PRBool ResizeOffscreen(const gfxIntSize& aNewSize) {
        if (mOffscreenFBO)
            return ResizeOffscreenFBO(aNewSize);
        return PR_FALSE;
    }

    /*
     * Return size of this offscreen context.
     *
     * Only valid if IsOffscreen() returns true.
     */
    gfxIntSize OffscreenSize() {
        return mOffscreenSize;
    }

    /*
     * In some cases, we have to allocate a bigger offscreen buffer
     * than what's requested.  This is the bigger size.
     *
     * Only valid if IsOffscreen() returns true.
     */
    gfxIntSize OffscreenActualSize() {
        return mOffscreenActualSize;
    }

    /*
     * If this context is FBO-backed, return the FBO or the color
     * buffer texture.  If the context is not FBO-backed, 0 is
     * returned (which is also a valid FBO binding).
     *
     * Only valid if IsOffscreen() returns true.
     */
    GLuint GetOffscreenFBO() {
        return mOffscreenFBO;
    }
    GLuint GetOffscreenTexture() {
        return mOffscreenTexture;
    }

    virtual PRBool TextureImageSupportsGetBackingSurface() {
        return PR_FALSE;
    }

    /**`
     * Return a valid, allocated TextureImage of |aSize| with
     * |aContentType|.  The TextureImage's texture is configured to
     * use |aWrapMode| (usually GL_CLAMP_TO_EDGE or GL_REPEAT) and by
     * default, GL_LINEAR filtering.  Specify
     * |aUseNearestFilter=PR_TRUE| for GL_NEAREST filtering.  Return
     * NULL if creating the TextureImage fails.
     *
     * The returned TextureImage may only be used with this GLContext.
     * Attempting to use the returned TextureImage after this
     * GLContext is destroyed will result in undefined (and likely
     * crashy) behavior.
     */
    virtual already_AddRefed<TextureImage>
    CreateTextureImage(const nsIntSize& aSize,
                       TextureImage::ContentType aContentType,
                       GLint aWrapMode,
                       PRBool aUseNearestFilter=PR_FALSE);

    /**
     * Read the image data contained in aTexture, and return it as an ImageSurface.
     * If GL_RGBA is given as the format, a ImageFormatARGB32 surface is returned.
     * Not implemented yet:
     * If GL_RGB is given as the format, a ImageFormatRGB24 surface is returned.
     * If GL_LUMINANCE is given as the format, a ImageFormatA8 surface is returned.
     *
     * THIS IS EXPENSIVE.  It is ridiculously expensive.  Only do this
     * if you absolutely positively must, and never in any performance
     * critical path.
     */
    already_AddRefed<gfxImageSurface> ReadTextureImage(GLuint aTexture,
                                                       const gfxIntSize& aSize,
                                                       GLenum aTextureFormat);

    /**
     * Call ReadPixels into an existing gfxImageSurface for the given bounds.
     * The image surface must be using image format RGBA32 or RGB24.
     */
    void ReadPixelsIntoImageSurface(GLint aX, GLint aY, GLsizei aWidth, GLsizei aHeight,
                                    gfxImageSurface *aDest);

    /**
     * Copy a rectangle from one TextureImage into another.  The
     * source and destination are given in integer coordinates, and
     * will be converted to texture coordinates.
     *
     * For the source texture, the wrap modes DO apply -- it's valid
     * to use REPEAT or PAD and expect appropriate behaviour if the source
     * rectangle extends beyond its bounds.
     *
     * For the destination texture, the wrap modes DO NOT apply -- the
     * destination will be clipped by the bounds of the texture.
     *
     * Note: calling this function will cause the following OpenGL state
     * to be changed:
     *
     *   - current program
     *   - framebuffer binding
     *   - viewport
     *   - blend state (will be enabled at end)
     *   - scissor state (will be enabled at end)
     *   - vertex attrib 0 and 1 (pointer and enable state [enable state will be disabled at exit])
     *   - array buffer binding (will be 0)
     *   - active texture (will be 0)
     *   - texture 0 binding
     */
    void BlitTextureImage(TextureImage *aSrc, const nsIntRect& aSrcRect,
                          TextureImage *aDst, const nsIntRect& aDstRect);

    /**
     * Known GL extensions that can be queried by
     * IsExtensionSupported.  The results of this are cached, and as
     * such it's safe to use this even in performance critical code.
     * If you add to this array, remember to add to the string names
     * in GLContext.cpp.
     */
    enum GLExtensions {
        EXT_framebuffer_object,
        ARB_framebuffer_object,
        ARB_texture_rectangle,
        EXT_bgra,
        EXT_texture_format_BGRA8888,
        OES_depth24,
        OES_depth32,
        OES_stencil8,
        OES_texture_npot,
        OES_depth_texture,
        OES_packed_depth_stencil,
        IMG_read_format,
        EXT_read_format_bgra,
        Extensions_Max
    };

    PRBool IsExtensionSupported(GLExtensions aKnownExtension) {
        return mAvailableExtensions[aKnownExtension];
    }

protected:
    PRPackedBool mInitialized;
    PRPackedBool mIsOffscreen;
    PRPackedBool mIsGLES2;
    PRPackedBool mIsGlobalSharedContext;
    PRPackedBool mWindowOriginBottomLeft;

    int mVendor;

    ContextFormat mCreationFormat;
    nsRefPtr<GLContext> mSharedContext;

    void UpdateActualFormat();
    ContextFormat mActualFormat;

    gfxIntSize mOffscreenSize;
    gfxIntSize mOffscreenActualSize;
    GLuint mOffscreenTexture;

    // lazy-initialized things
    GLuint mBlitProgram, mBlitFramebuffer;
    void UseBlitProgram();
    void SetBlitFramebufferForDestTexture(GLuint aTexture);

    // helper to create/resize an offscreen FBO,
    // for offscreen implementations that use FBOs.
    PRBool ResizeOffscreenFBO(const gfxIntSize& aSize);
    void DeleteOffscreenFBO();
    GLuint mOffscreenFBO;
    GLuint mOffscreenDepthRB;
    GLuint mOffscreenStencilRB;

    // this should just be a std::bitset, but that ended up breaking
    // MacOS X builds; see bug 584919.  We can replace this with one
    // later on.
    template<size_t setlen>
    struct ExtensionBitset {
        ExtensionBitset() {
            for (size_t i = 0; i < setlen; ++i)
                values[i] = false;
        }

        bool& operator[](size_t index) {
            NS_ASSERTION(index < setlen, "out of range");
            return values[index];
        }

        bool values[setlen];
    };
    ExtensionBitset<Extensions_Max> mAvailableExtensions;

    // Clear to transparent black, with 0 depth and stencil,
    // while preserving current ClearColor etc. values.
    // Useful for resizing offscreen buffers.
    void ClearSafely();

    nsDataHashtable<nsVoidPtrHashKey, void*> mUserData;

    void SetIsGLES2(PRBool aIsGLES2) {
        NS_ASSERTION(!mInitialized, "SetIsGLES2 can only be called before initialization!");
        mIsGLES2 = aIsGLES2;
    }

    PRBool InitWithPrefix(const char *prefix, PRBool trygl);

    void InitExtensions();
    PRBool IsExtensionSupported(const char *extension);

    virtual already_AddRefed<TextureImage>
    CreateBasicTextureImage(GLuint aTexture,
                            const nsIntSize& aSize,
                            TextureImage::ContentType aContentType,
                            GLContext* aContext)
    { return NULL; }

    //
    // the wrapped functions
    //
public:
    /* One would think that this would live in some nice
     * perl-or-python-or-js script somewhere and would be
     * autogenerated; one would be wrong.
     */
    // Keep this at the start of the function pointers
    void *mFunctionListStartSentinel;

    typedef void (GLAPIENTRY * PFNGLACTIVETEXTUREPROC) (GLenum texture);
    PFNGLACTIVETEXTUREPROC fActiveTexture;
    typedef void (GLAPIENTRY * PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
    PFNGLATTACHSHADERPROC fAttachShader;
    typedef void (GLAPIENTRY * PFNGLBINDATTRIBLOCATIONPROC) (GLuint program, GLuint index, const GLchar* name);
    PFNGLBINDATTRIBLOCATIONPROC fBindAttribLocation;
    typedef void (GLAPIENTRY * PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
    PFNGLBINDBUFFERPROC fBindBuffer;
    typedef void (GLAPIENTRY * PFNGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
    PFNGLBINDTEXTUREPROC fBindTexture;
    typedef void (GLAPIENTRY * PFNGLBLENDCOLORPROC) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
    PFNGLBLENDCOLORPROC fBlendColor;
    typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONPROC) (GLenum mode);
    PFNGLBLENDEQUATIONPROC fBlendEquation;
    typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONSEPARATEPROC) (GLenum, GLenum);
    PFNGLBLENDEQUATIONSEPARATEPROC fBlendEquationSeparate;
    typedef void (GLAPIENTRY * PFNGLBLENDFUNCPROC) (GLenum, GLenum);
    PFNGLBLENDFUNCPROC fBlendFunc;
    typedef void (GLAPIENTRY * PFNGLBLENDFUNCSEPARATEPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
    PFNGLBLENDFUNCSEPARATEPROC fBlendFuncSeparate;
    typedef void (GLAPIENTRY * PFNGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
    PFNGLBUFFERDATAPROC fBufferData;
    typedef void (GLAPIENTRY * PFNGLBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);
    PFNGLBUFFERSUBDATAPROC fBufferSubData;
    typedef void (GLAPIENTRY * PFNGLCLEARPROC) (GLbitfield);
    PFNGLCLEARPROC fClear;
    typedef void (GLAPIENTRY * PFNGLCLEARCOLORPROC) (GLclampf, GLclampf, GLclampf, GLclampf);
    PFNGLCLEARCOLORPROC fClearColor;
    typedef void (GLAPIENTRY * PFNGLCLEARSTENCILPROC) (GLint);
    PFNGLCLEARSTENCILPROC fClearStencil;
    typedef void (GLAPIENTRY * PFNGLCOLORMASKPROC) (realGLboolean red, realGLboolean green, realGLboolean blue, realGLboolean alpha);
    PFNGLCOLORMASKPROC fColorMask;
    typedef void (GLAPIENTRY * PFNGLCULLFACEPROC) (GLenum mode);
    PFNGLCULLFACEPROC fCullFace;
    typedef void (GLAPIENTRY * PFNGLDETACHSHADERPROC) (GLuint program, GLuint shader);
    PFNGLDETACHSHADERPROC fDetachShader;
    typedef void (GLAPIENTRY * PFNGLDEPTHFUNCPROC) (GLenum);
    PFNGLDEPTHFUNCPROC fDepthFunc;
    typedef void (GLAPIENTRY * PFNGLDEPTHMASKPROC) (realGLboolean);
    PFNGLDEPTHMASKPROC fDepthMask;
    typedef void (GLAPIENTRY * PFNGLDISABLEPROC) (GLenum);
    PFNGLDISABLEPROC fDisable;
    typedef void (GLAPIENTRY * PFNGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint);
    PFNGLDISABLEVERTEXATTRIBARRAYPROC fDisableVertexAttribArray;
    typedef void (GLAPIENTRY * PFNGLDRAWARRAYSPROC) (GLenum mode, GLint first, GLsizei count);
    PFNGLDRAWARRAYSPROC fDrawArrays;
    typedef void (GLAPIENTRY * PFNGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
    PFNGLDRAWELEMENTSPROC fDrawElements;
    typedef void (GLAPIENTRY * PFNGLENABLEPROC) (GLenum);
    PFNGLENABLEPROC fEnable;
    typedef void (GLAPIENTRY * PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint);
    PFNGLENABLEVERTEXATTRIBARRAYPROC fEnableVertexAttribArray;
    typedef void (GLAPIENTRY * PFNGLFINISHPROC) (void);
    PFNGLFINISHPROC fFinish;
    typedef void (GLAPIENTRY * PFNGLFLUSHPROC) (void);
    PFNGLFLUSHPROC fFlush;
    typedef void (GLAPIENTRY * PFNGLFRONTFACEPROC) (GLenum);
    PFNGLFRONTFACEPROC fFrontFace;
    typedef void (GLAPIENTRY * PFNGLGETACTIVEATTRIBPROC) (GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
    PFNGLGETACTIVEATTRIBPROC fGetActiveAttrib;
    typedef void (GLAPIENTRY * PFNGLGETACTIVEUNIFORMPROC) (GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
    PFNGLGETACTIVEUNIFORMPROC fGetActiveUniform;
    typedef void (GLAPIENTRY * PFNGLGETATTACHEDSHADERSPROC) (GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders);
    PFNGLGETATTACHEDSHADERSPROC fGetAttachedShaders;
    typedef GLint (GLAPIENTRY * PFNGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar* name);
    PFNGLGETATTRIBLOCATIONPROC fGetAttribLocation;
    typedef void (GLAPIENTRY * PFNGLGETINTEGERVPROC) (GLenum pname, GLint *params);
    PFNGLGETINTEGERVPROC fGetIntegerv;
    typedef void (GLAPIENTRY * PFNGLGETFLOATVPROC) (GLenum pname, GLfloat *params);
    PFNGLGETFLOATVPROC fGetFloatv;
    typedef void (GLAPIENTRY * PFNGLGETBOOLEANBPROC) (GLenum pname, realGLboolean *params);
    PFNGLGETBOOLEANBPROC fGetBooleanv;
    typedef void (GLAPIENTRY * PFNGLGETBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint* params);
    PFNGLGETBUFFERPARAMETERIVPROC fGetBufferParameteriv;
    typedef void (GLAPIENTRY * PFNGLGENERATEMIPMAPPROC) (GLenum target);
    PFNGLGENERATEMIPMAPPROC fGenerateMipmap;
    typedef GLenum (GLAPIENTRY * PFNGLGETERRORPROC) (void);
    PFNGLGETERRORPROC fGetError;
    typedef void (GLAPIENTRY * PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint* param);
    PFNGLGETPROGRAMIVPROC fGetProgramiv;
    typedef void (GLAPIENTRY * PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
    PFNGLGETPROGRAMINFOLOGPROC fGetProgramInfoLog;
    typedef void (GLAPIENTRY * PFNGLTEXPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);
    PFNGLTEXPARAMETERIPROC fTexParameteri;
    typedef void (GLAPIENTRY * PFNGLTEXPARAMETERFPROC) (GLenum target, GLenum pname, GLfloat param);
    PFNGLTEXPARAMETERFPROC fTexParameterf;
    typedef GLubyte* (GLAPIENTRY * PFNGLGETSTRINGPROC) (GLenum);
    PFNGLGETSTRINGPROC fGetString;
    typedef void (GLAPIENTRY * PFNGLGETTEXPARAMETERFVPROC) (GLenum target, GLenum pname, const GLfloat *params);
    PFNGLGETTEXPARAMETERFVPROC fGetTexParameterfv;
    typedef void (GLAPIENTRY * PFNGLGETTEXPARAMETERIVPROC) (GLenum target, GLenum pname, const GLint *params);
    PFNGLGETTEXPARAMETERIVPROC fGetTexParameteriv;
    typedef void (GLAPIENTRY * PFNGLGETUNIFORMFVPROC) (GLuint program, GLint location, GLfloat* params);
    PFNGLGETUNIFORMFVPROC fGetUniformfv;
    typedef void (GLAPIENTRY * PFNGLGETUNIFORMIVPROC) (GLuint program, GLint location, GLint* params);
    PFNGLGETUNIFORMIVPROC fGetUniformiv;
    typedef GLint (GLAPIENTRY * PFNGLGETUNIFORMLOCATIONPROC) (GLint programObj, const GLchar* name);
    PFNGLGETUNIFORMLOCATIONPROC fGetUniformLocation;
    typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBFVPROC) (GLuint, GLenum, GLfloat*);
    PFNGLGETVERTEXATTRIBFVPROC fGetVertexAttribfv;
    typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBIVPROC) (GLuint, GLenum, GLint*);
    PFNGLGETVERTEXATTRIBIVPROC fGetVertexAttribiv;
    typedef void (GLAPIENTRY * PFNGLHINTPROC) (GLenum target, GLenum mode);
    PFNGLHINTPROC fHint;
    typedef realGLboolean (GLAPIENTRY * PFNGLISBUFFERPROC) (GLuint buffer);
    PFNGLISBUFFERPROC fIsBuffer;
    typedef realGLboolean (GLAPIENTRY * PFNGLISENABLEDPROC) (GLenum cap);
    PFNGLISENABLEDPROC fIsEnabled;
    typedef realGLboolean (GLAPIENTRY * PFNGLISPROGRAMPROC) (GLuint program);
    PFNGLISPROGRAMPROC fIsProgram;
    typedef realGLboolean (GLAPIENTRY * PFNGLISSHADERPROC) (GLuint shader);
    PFNGLISSHADERPROC fIsShader;
    typedef realGLboolean (GLAPIENTRY * PFNGLISTEXTUREPROC) (GLuint texture);
    PFNGLISTEXTUREPROC fIsTexture;
    typedef void (GLAPIENTRY * PFNGLLINEWIDTHPROC) (GLfloat width);
    PFNGLLINEWIDTHPROC fLineWidth;
    typedef void (GLAPIENTRY * PFNGLLINKPROGRAMPROC) (GLuint program);
    PFNGLLINKPROGRAMPROC fLinkProgram;
    typedef void (GLAPIENTRY * PFNGLPIXELSTOREIPROC) (GLenum pname, GLint param);
    PFNGLPIXELSTOREIPROC fPixelStorei;
    typedef void (GLAPIENTRY * PFNGLPOLYGONOFFSETPROC) (GLfloat factor, GLfloat bias);
    PFNGLPOLYGONOFFSETPROC fPolygonOffset;
    typedef void (GLAPIENTRY * PFNGLREADBUFFERPROC) (GLenum);
    PFNGLREADBUFFERPROC fReadBuffer;
    typedef void (GLAPIENTRY * PFNGLREADPIXELSPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
    PFNGLREADPIXELSPROC fReadPixels;
    typedef void (GLAPIENTRY * PFNGLSAMPLECOVERAGEPROC) (GLclampf value, realGLboolean invert);
    PFNGLSAMPLECOVERAGEPROC fSampleCoverage;
    typedef void (GLAPIENTRY * PFNGLSTENCILFUNCPROC) (GLenum func, GLint ref, GLuint mask);
    PFNGLSTENCILFUNCPROC fStencilFunc;
    typedef void (GLAPIENTRY * PFNGLSTENCILFUNCSEPARATEPROC) (GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask);
    PFNGLSTENCILFUNCSEPARATEPROC fStencilFuncSeparate;
    typedef void (GLAPIENTRY * PFNGLSTENCILMASKPROC) (GLuint mask);
    PFNGLSTENCILMASKPROC fStencilMask;
    typedef void (GLAPIENTRY * PFNGLSTENCILMASKSEPARATEPROC) (GLenum, GLuint);
    PFNGLSTENCILMASKSEPARATEPROC fStencilMaskSeparate;
    typedef void (GLAPIENTRY * PFNGLSTENCILOPPROC) (GLenum fail, GLenum zfail, GLenum zpass);
    PFNGLSTENCILOPPROC fStencilOp;
    typedef void (GLAPIENTRY * PFNGLSTENCILOPSEPARATEPROC) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
    PFNGLSTENCILOPSEPARATEPROC fStencilOpSeparate;
    typedef void (GLAPIENTRY * PFNGLTEXIMAGE2DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
    PFNGLTEXIMAGE2DPROC fTexImage2D;
    typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
    PFNGLTEXSUBIMAGE2DPROC fTexSubImage2D;
    typedef void (GLAPIENTRY * PFNGLUNIFORM1FPROC) (GLint location, GLfloat v0);
    PFNGLUNIFORM1FPROC fUniform1f;
    typedef void (GLAPIENTRY * PFNGLUNIFORM1FVPROC) (GLint location, GLsizei count, const GLfloat* value);
    PFNGLUNIFORM1FVPROC fUniform1fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
    PFNGLUNIFORM1IPROC fUniform1i;
    typedef void (GLAPIENTRY * PFNGLUNIFORM1IVPROC) (GLint location, GLsizei count, const GLint* value);
    PFNGLUNIFORM1IVPROC fUniform1iv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM2FPROC) (GLint location, GLfloat v0, GLfloat v1);
    PFNGLUNIFORM2FPROC fUniform2f;
    typedef void (GLAPIENTRY * PFNGLUNIFORM2FVPROC) (GLint location, GLsizei count, const GLfloat* value);
    PFNGLUNIFORM2FVPROC fUniform2fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM2IPROC) (GLint location, GLint v0, GLint v1);
    PFNGLUNIFORM2IPROC fUniform2i;
    typedef void (GLAPIENTRY * PFNGLUNIFORM2IVPROC) (GLint location, GLsizei count, const GLint* value);
    PFNGLUNIFORM2IVPROC fUniform2iv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM3FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    PFNGLUNIFORM3FPROC fUniform3f;
    typedef void (GLAPIENTRY * PFNGLUNIFORM3FVPROC) (GLint location, GLsizei count, const GLfloat* value);
    PFNGLUNIFORM3FVPROC fUniform3fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM3IPROC) (GLint location, GLint v0, GLint v1, GLint v2);
    PFNGLUNIFORM3IPROC fUniform3i;
    typedef void (GLAPIENTRY * PFNGLUNIFORM3IVPROC) (GLint location, GLsizei count, const GLint* value);
    PFNGLUNIFORM3IVPROC fUniform3iv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM4FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    PFNGLUNIFORM4FPROC fUniform4f;
    typedef void (GLAPIENTRY * PFNGLUNIFORM4FVPROC) (GLint location, GLsizei count, const GLfloat* value);
    PFNGLUNIFORM4FVPROC fUniform4fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORM4IPROC) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
    PFNGLUNIFORM4IPROC fUniform4i;
    typedef void (GLAPIENTRY * PFNGLUNIFORM4IVPROC) (GLint location, GLsizei count, const GLint* value);
    PFNGLUNIFORM4IVPROC fUniform4iv;
    typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX2FVPROC) (GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value);
    PFNGLUNIFORMMATRIX2FVPROC fUniformMatrix2fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX3FVPROC) (GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value);
    PFNGLUNIFORMMATRIX3FVPROC fUniformMatrix3fv;
    typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX4FVPROC) (GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value);
    PFNGLUNIFORMMATRIX4FVPROC fUniformMatrix4fv;
    typedef void (GLAPIENTRY * PFNGLUSEPROGRAMPROC) (GLuint program);
    PFNGLUSEPROGRAMPROC fUseProgram;
    typedef void (GLAPIENTRY * PFNGLVALIDATEPROGRAMPROC) (GLuint program);
    PFNGLVALIDATEPROGRAMPROC fValidateProgram;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, realGLboolean normalized, GLsizei stride, const GLvoid* pointer);
    PFNGLVERTEXATTRIBPOINTERPROC fVertexAttribPointer;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1FPROC) (GLuint index, GLfloat x);
    PFNGLVERTEXATTRIB1FPROC fVertexAttrib1f;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2FPROC) (GLuint index, GLfloat x, GLfloat y);
    PFNGLVERTEXATTRIB2FPROC fVertexAttrib2f;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
    PFNGLVERTEXATTRIB3FPROC fVertexAttrib3f;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    PFNGLVERTEXATTRIB4FPROC fVertexAttrib4f;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1FVPROC) (GLuint index, const GLfloat* v);
    PFNGLVERTEXATTRIB1FVPROC fVertexAttrib1fv;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2FVPROC) (GLuint index, const GLfloat* v);
    PFNGLVERTEXATTRIB2FVPROC fVertexAttrib2fv;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3FVPROC) (GLuint index, const GLfloat* v);
    PFNGLVERTEXATTRIB3FVPROC fVertexAttrib3fv;
    typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4FVPROC) (GLuint index, const GLfloat* v);
    PFNGLVERTEXATTRIB4FVPROC fVertexAttrib4fv;
    typedef void (GLAPIENTRY * PFNGLCOMPILESHADERPROC) (GLuint shader);
    PFNGLCOMPILESHADERPROC fCompileShader;
    typedef void (GLAPIENTRY * PFNGLCOPYTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
    PFNGLCOPYTEXIMAGE2DPROC fCopyTexImage2D;
    typedef void (GLAPIENTRY * PFNGLCOPYTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
    PFNGLCOPYTEXSUBIMAGE2DPROC fCopyTexSubImage2D;
    typedef void (GLAPIENTRY * PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint* param);
    PFNGLGETSHADERIVPROC fGetShaderiv;
    typedef void (GLAPIENTRY * PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
    PFNGLGETSHADERINFOLOGPROC fGetShaderInfoLog;
    typedef void (GLAPIENTRY * PFNGLGETSHADERSOURCEPROC) (GLint obj, GLsizei maxLength, GLsizei* length, GLchar* source);
    PFNGLGETSHADERSOURCEPROC fGetShaderSource;
    typedef void (GLAPIENTRY * PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar** strings, const GLint* lengths);
    PFNGLSHADERSOURCEPROC fShaderSource;

    typedef void (GLAPIENTRY * PFNGLBINDFRAMEBUFFER) (GLenum target, GLuint framebuffer);
    PFNGLBINDFRAMEBUFFER fBindFramebuffer;
    typedef void (GLAPIENTRY * PFNGLBINDRENDERBUFFER) (GLenum target, GLuint renderbuffer);
    PFNGLBINDRENDERBUFFER fBindRenderbuffer;
    typedef GLenum (GLAPIENTRY * PFNGLCHECKFRAMEBUFFERSTATUS) (GLenum target);
    PFNGLCHECKFRAMEBUFFERSTATUS fCheckFramebufferStatus;
    typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERRENDERBUFFER) (GLenum target, GLenum attachmentPoint, GLenum renderbufferTarget, GLuint renderbuffer);
    PFNGLFRAMEBUFFERRENDERBUFFER fFramebufferRenderbuffer;
    typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE2D) (GLenum target, GLenum attachmentPoint, GLenum textureTarget, GLuint texture, GLint level);
    PFNGLFRAMEBUFFERTEXTURE2D fFramebufferTexture2D;
    typedef void (GLAPIENTRY * PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIV) (GLenum target, GLenum attachment, GLenum pname, GLint* value);
    PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIV fGetFramebufferAttachmentParameteriv;
    typedef void (GLAPIENTRY * PFNGLGETRENDERBUFFERPARAMETERIV) (GLenum target, GLenum pname, GLint* value);
    PFNGLGETRENDERBUFFERPARAMETERIV fGetRenderbufferParameteriv;
    typedef realGLboolean (GLAPIENTRY * PFNGLISFRAMEBUFFER) (GLuint framebuffer);
    PFNGLISFRAMEBUFFER fIsFramebuffer;
    typedef realGLboolean (GLAPIENTRY * PFNGLISRENDERBUFFER) (GLuint renderbuffer);
    PFNGLISRENDERBUFFER fIsRenderbuffer;
    typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGE) (GLenum target, GLenum internalFormat, GLsizei width, GLsizei height);
    PFNGLRENDERBUFFERSTORAGE fRenderbufferStorage;

    // keep this at the end of the function pointers
    void *mFunctionListEndSentinel;

    void fDepthRange(GLclampf a, GLclampf b) {
        if (mIsGLES2) {
            priv_fDepthRangef(a, b);
        } else {
            priv_fDepthRange(a, b);
        }
    }

    void fClearDepth(GLclampf v) {
        if (mIsGLES2) {
            priv_fClearDepthf(v);
        } else {
            priv_fClearDepth(v);
        }
    }

    void fScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
        ScissorRect().SetRect(x, y, width, height);
        priv_fScissor(x, y, width, height);
    }

    nsIntRect& ScissorRect() {
        return mScissorStack[mScissorStack.Length()-1];
    }

    void PushScissorRect() {
        mScissorStack.AppendElement(ScissorRect());
    }

    void PushScissorRect(const nsIntRect& aRect) {
        mScissorStack.AppendElement(aRect);
        priv_fScissor(aRect.x, aRect.y, aRect.width, aRect.height);
    }

    void PopScissorRect() {
        if (mScissorStack.Length() < 2) {
            NS_WARNING("PopScissorRect with Length < 2!");
            return;
        }

        nsIntRect thisRect = ScissorRect();
        mScissorStack.TruncateLength(mScissorStack.Length() - 1);
        if (thisRect != ScissorRect()) {
            priv_fScissor(ScissorRect().x, ScissorRect().y,
                          ScissorRect().width, ScissorRect().height);
        }
    }

    void fViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
        ViewportRect().SetRect(x, y, width, height);
        priv_fViewport(x, y, width, height);
    }

    nsIntRect& ViewportRect() {
        return mViewportStack[mViewportStack.Length()-1];
    }

    void PushViewportRect() {
        mViewportStack.AppendElement(ViewportRect());
    }

    void PushViewportRect(const nsIntRect& aRect) {
        mViewportStack.AppendElement(aRect);
        priv_fViewport(aRect.x, aRect.y, aRect.width, aRect.height);
    }

    void PopViewportRect() {
        if (mViewportStack.Length() < 2) {
            NS_WARNING("PopViewportRect with Length < 2!");
            return;
        }

        nsIntRect thisRect = ViewportRect();
        mViewportStack.TruncateLength(mViewportStack.Length() - 1);
        if (thisRect != ViewportRect()) {
            priv_fViewport(ViewportRect().x, ViewportRect().y,
                           ViewportRect().width, ViewportRect().height);
        }
    }

protected:
    nsTArray<nsIntRect> mViewportStack;
    nsTArray<nsIntRect> mScissorStack;

    /* These are different between GLES2 and desktop GL; we hide those differences, use the GL
     * names, but the most limited data type.
     */
    typedef void (GLAPIENTRY * PFNGLDEPTHRANGEFPROC) (GLclampf, GLclampf);
    PFNGLDEPTHRANGEFPROC priv_fDepthRangef;
    typedef void (GLAPIENTRY * PFNGLCLEARDEPTHFPROC) (GLclampf);
    PFNGLCLEARDEPTHFPROC priv_fClearDepthf;

    typedef void (GLAPIENTRY * PFNGLDEPTHRANGEPROC) (GLclampd, GLclampd);
    PFNGLDEPTHRANGEPROC priv_fDepthRange;
    typedef void (GLAPIENTRY * PFNGLCLEARDEPTHPROC) (GLclampd);
    PFNGLCLEARDEPTHPROC priv_fClearDepth;

    /* These are special because we end up tracking these so that we don't
     * have to query the values from GL.
     */

    typedef void (GLAPIENTRY * PFNGLVIEWPORTPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
    PFNGLVIEWPORTPROC priv_fViewport;
    typedef void (GLAPIENTRY * PFNGLSCISSORPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
    PFNGLSCISSORPROC priv_fScissor;


    /* These are special -- they create or delete GL resources that can live
     * in a shared namespace.  In DEBUG, we wrap these calls so that we can
     * check when we have something that failed to do cleanup at the time the
     * final context is destroyed.
     */

    typedef GLuint (GLAPIENTRY * PFNGLCREATEPROGRAMPROC) (void);
    PFNGLCREATEPROGRAMPROC priv_fCreateProgram;
    typedef GLuint (GLAPIENTRY * PFNGLCREATESHADERPROC) (GLenum type);
    PFNGLCREATESHADERPROC priv_fCreateShader;
    typedef void (GLAPIENTRY * PFNGLGENBUFFERSPROC) (GLsizei n, GLuint* buffers);
    PFNGLGENBUFFERSPROC priv_fGenBuffers;
    typedef void (GLAPIENTRY * PFNGLGENTEXTURESPROC) (GLsizei n, GLuint *textures);
    PFNGLGENTEXTURESPROC priv_fGenTextures;
    typedef void (GLAPIENTRY * PFNGLGENFRAMEBUFFERS) (GLsizei n, GLuint* ids);
    PFNGLGENFRAMEBUFFERS priv_fGenFramebuffers;
    typedef void (GLAPIENTRY * PFNGLGENRENDERBUFFERS) (GLsizei n, GLuint* ids);
    PFNGLGENRENDERBUFFERS priv_fGenRenderbuffers;

    typedef void (GLAPIENTRY * PFNGLDELETEPROGRAMPROC) (GLuint program);
    PFNGLDELETEPROGRAMPROC priv_fDeleteProgram;
    typedef void (GLAPIENTRY * PFNGLDELETESHADERPROC) (GLuint shader);
    PFNGLDELETESHADERPROC priv_fDeleteShader;
    typedef void (GLAPIENTRY * PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint* buffers);
    PFNGLDELETEBUFFERSPROC priv_fDeleteBuffers;
    typedef void (GLAPIENTRY * PFNGLDELETETEXTURESPROC) (GLsizei n, const GLuint* textures);
    PFNGLDELETETEXTURESPROC priv_fDeleteTextures;
    typedef void (GLAPIENTRY * PFNGLDELETEFRAMEBUFFERS) (GLsizei n, const GLuint* ids);
    PFNGLDELETEFRAMEBUFFERS priv_fDeleteFramebuffers;
    typedef void (GLAPIENTRY * PFNGLDELETERENDERBUFFERS) (GLsizei n, const GLuint* ids);
    PFNGLDELETERENDERBUFFERS priv_fDeleteRenderbuffers;

public:
#ifndef DEBUG
    GLuint GLAPIENTRY fCreateProgram() {
        return priv_fCreateProgram();
    }

    GLuint GLAPIENTRY fCreateShader(GLenum t) {
        return priv_fCreateShader(t);
    }

    void GLAPIENTRY fGenBuffers(GLsizei n, GLuint* names) {
        priv_fGenBuffers(n, names);
    }

    void GLAPIENTRY fGenTextures(GLsizei n, GLuint* names) {
        priv_fGenTextures(n, names);
    }

    void GLAPIENTRY fGenFramebuffers(GLsizei n, GLuint* names) {
        priv_fGenFramebuffers(n, names);
    }

    void GLAPIENTRY fGenRenderbuffers(GLsizei n, GLuint* names) {
        priv_fGenRenderbuffers(n, names);
    }

    void GLAPIENTRY fDeleteProgram(GLuint program) {
        priv_fDeleteProgram(program);
    }

    void GLAPIENTRY fDeleteShader(GLuint shader) {
        priv_fDeleteShader(shader);
    }

    void GLAPIENTRY fDeleteBuffers(GLsizei n, GLuint *names) {
        priv_fDeleteBuffers(n, names);
    }

    void GLAPIENTRY fDeleteTextures(GLsizei n, GLuint *names) {
        priv_fDeleteTextures(n, names);
    }

    void GLAPIENTRY fDeleteFramebuffers(GLsizei n, GLuint *names) {
        priv_fDeleteFramebuffers(n, names);
    }

    void GLAPIENTRY fDeleteRenderbuffers(GLsizei n, GLuint *names) {
        priv_fDeleteRenderbuffers(n, names);
    }
#else
    GLContext *TrackingContext() {
        GLContext *tip = this;
        while (tip->mSharedContext)
            tip = tip->mSharedContext;
        return tip;
    }

    GLuint GLAPIENTRY fCreateProgram() {
        GLuint ret = priv_fCreateProgram();
        TrackingContext()->CreatedProgram(this, ret);
        return ret;
    }

    GLuint GLAPIENTRY fCreateShader(GLenum t) {
        GLuint ret = priv_fCreateShader(t);
        TrackingContext()->CreatedShader(this, ret);
        return ret;
    }

    void GLAPIENTRY fGenBuffers(GLsizei n, GLuint* names) {
        priv_fGenBuffers(n, names);
        TrackingContext()->CreatedBuffers(this, n, names);
    }

    void GLAPIENTRY fGenTextures(GLsizei n, GLuint* names) {
        priv_fGenTextures(n, names);
        TrackingContext()->CreatedTextures(this, n, names);
    }

    void GLAPIENTRY fGenFramebuffers(GLsizei n, GLuint* names) {
        priv_fGenFramebuffers(n, names);
        TrackingContext()->CreatedFramebuffers(this, n, names);
    }

    void GLAPIENTRY fGenRenderbuffers(GLsizei n, GLuint* names) {
        priv_fGenRenderbuffers(n, names);
        TrackingContext()->CreatedRenderbuffers(this, n, names);
    }

    void GLAPIENTRY fDeleteProgram(GLuint program) {
        priv_fDeleteProgram(program);
        TrackingContext()->DeletedProgram(this, program);
    }

    void GLAPIENTRY fDeleteShader(GLuint shader) {
        priv_fDeleteShader(shader);
        TrackingContext()->DeletedShader(this, shader);
    }

    void GLAPIENTRY fDeleteBuffers(GLsizei n, GLuint *names) {
        priv_fDeleteBuffers(n, names);
        TrackingContext()->DeletedBuffers(this, n, names);
    }

    void GLAPIENTRY fDeleteTextures(GLsizei n, GLuint *names) {
        priv_fDeleteTextures(n, names);
        TrackingContext()->DeletedTextures(this, n, names);
    }

    void GLAPIENTRY fDeleteFramebuffers(GLsizei n, GLuint *names) {
        priv_fDeleteFramebuffers(n, names);
        TrackingContext()->DeletedFramebuffers(this, n, names);
    }

    void GLAPIENTRY fDeleteRenderbuffers(GLsizei n, GLuint *names) {
        priv_fDeleteRenderbuffers(n, names);
        TrackingContext()->DeletedRenderbuffers(this, n, names);
    }

    void THEBES_API CreatedProgram(GLContext *aOrigin, GLuint aName);
    void THEBES_API CreatedShader(GLContext *aOrigin, GLuint aName);
    void THEBES_API CreatedBuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void THEBES_API CreatedTextures(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void THEBES_API CreatedFramebuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void THEBES_API CreatedRenderbuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void THEBES_API DeletedProgram(GLContext *aOrigin, GLuint aName);
    void THEBES_API DeletedShader(GLContext *aOrigin, GLuint aName);
    void THEBES_API DeletedBuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void THEBES_API DeletedTextures(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void THEBES_API DeletedFramebuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);
    void THEBES_API DeletedRenderbuffers(GLContext *aOrigin, GLsizei aCount, GLuint *aNames);

    void SharedContextDestroyed(GLContext *aChild);
    void ReportOutstandingNames();

    struct NamedResource {
        NamedResource()
            : origin(nsnull), name(0), originDeleted(PR_FALSE)
        { }

        NamedResource(GLContext *aOrigin, GLuint aName)
            : origin(aOrigin), name(aName), originDeleted(PR_FALSE)
        { }

        GLContext *origin;
        GLuint name;
        PRBool originDeleted;

        // for sorting
        bool operator<(const NamedResource& aOther) const {
            if (intptr_t(origin) < intptr_t(aOther.origin))
                return true;
            if (name < aOther.name)
                return true;
            return false;
        }
        bool operator==(const NamedResource& aOther) const {
            return origin == aOther.origin &&
                name == aOther.name &&
                originDeleted == aOther.originDeleted;
        }
    };

    nsTArray<NamedResource> mTrackedPrograms;
    nsTArray<NamedResource> mTrackedShaders;
    nsTArray<NamedResource> mTrackedTextures;
    nsTArray<NamedResource> mTrackedFramebuffers;
    nsTArray<NamedResource> mTrackedRenderbuffers;
    nsTArray<NamedResource> mTrackedBuffers;
#endif

};

inline void
GLDebugPrintError(GLContext* aCx, const char* const aFile, int aLine)
{
    aCx->MakeCurrent();
    GLenum err = aCx->fGetError();
    if (err) {
        printf_stderr("GL ERROR: 0x%04x at %s:%d\n", err, aFile, aLine);
    }
}

#ifdef DEBUG
#  define DEBUG_GL_ERROR_CHECK(cx) mozilla::gl::GLDebugPrintError(cx, __FILE__, __LINE__)
#else
#  define DEBUG_GL_ERROR_CHECK(cx) do { } while (0)
#endif

inline PRBool
DoesVendorStringMatch(const char* aVendorString, const char *aWantedVendor)
{
    const char *occurrence = strstr(aVendorString, aWantedVendor);

    // aWantedVendor not found
    if (!occurrence)
        return PR_FALSE;

    // aWantedVendor preceded by alpha character
    if (occurrence != aVendorString && isalpha(*(occurrence-1)))
        return PR_FALSE;

    // aWantedVendor followed by alpha character
    const char *afterOccurrence = occurrence + strlen(aWantedVendor);
    if (isalpha(*afterOccurrence))
        return PR_FALSE;

    return PR_TRUE;
}

} /* namespace gl */
} /* namespace mozilla */

#endif /* GLCONTEXT_H_ */
