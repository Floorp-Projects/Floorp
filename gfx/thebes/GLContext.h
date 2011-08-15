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
#if defined(XP_UNIX)
#include <stdint.h>
#endif
#include <string.h>
#include <ctype.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "GLDefs.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "gfxRect.h"
#include "nsISupportsImpl.h"
#include "prlink.h"

#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsRegion.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"

#if defined(MOZ_PLATFORM_MAEMO) || defined(ANDROID) || defined(MOZ_EGL_XRENDER_COMPOSITE)
#define USE_GLES2 1
#endif

typedef char realGLboolean;

#include "GLContextSymbols.h"

namespace mozilla {
  namespace layers {
    class LayerManagerOGL;
    class ColorTextureLayerProgram;
  };

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

enum ShaderProgramType {
    RGBALayerProgramType,
    BGRALayerProgramType,
    RGBXLayerProgramType,
    BGRXLayerProgramType,
    RGBARectLayerProgramType,
    ColorLayerProgramType,
    YCbCrLayerProgramType,
    ComponentAlphaPass1ProgramType,
    ComponentAlphaPass2ProgramType,
    Copy2DProgramType,
    Copy2DRectProgramType,
    NumProgramTypes
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
    enum TextureState
    {
      Created, // Texture created, but has not had glTexImage called to initialize it.
      Allocated,  // Texture memory exists, but contents are invalid.
      Valid  // Texture fully ready to use.
    };

    typedef gfxASurface::gfxContentType ContentType;

    virtual ~TextureImage() {}

    /**
     * Returns a gfxASurface for updating |aRegion| of the client's
     * image if successul, NULL if not.  |aRegion|'s bounds must fit
     * within Size(); its coordinate space (if any) is ignored.  If
     * the update begins successfully, the returned gfxASurface is
     * owned by this.  Otherwise, NULL is returned.
     *
     * |aRegion| is an inout param: the returned region is what the
     * client must repaint.  Category (1) regions above can
     * efficiently handle repaints to "scattered" regions, while (2)
     * can only efficiently handle repaints to rects.
     *
     * Painting the returned surface outside of |aRegion| results 
     * in undefined behavior.
     *
     * BeginUpdate() calls cannot be "nested", and each successful
     * BeginUpdate() must be followed by exactly one EndUpdate() (see
     * below).  Failure to do so can leave this in a possibly
     * inconsistent state.  Unsuccessful BeginUpdate()s must not be
     * followed by EndUpdate().
     */
    virtual gfxASurface* BeginUpdate(nsIntRegion& aRegion) = 0;
    /**
     * Finish the active update and synchronize with the server, if
     * necessary.
     *
     * BeginUpdate() must have been called exactly once before
     * EndUpdate().
     */
    virtual void EndUpdate() = 0;

    /**
     * The Image may contain several textures for different regions (tiles).
     * These functions iterate over each sub texture image tile.
     */
    virtual void BeginTileIteration() {
    };

    virtual PRBool NextTile() {
        return PR_FALSE;
    };

    virtual nsIntRect GetTileRect() {
        return nsIntRect(nsIntPoint(0,0), mSize);
    };

    virtual GLuint GetTextureID() = 0;
    /**
     * Set this TextureImage's size, and ensure a texture has been
     * allocated.  Must not be called between BeginUpdate and EndUpdate.
     * After a resize, the contents are undefined.
     *
     * If this isn't implemented by a subclass, it will just perform
     * a dummy BeginUpdate/EndUpdate pair.
     */
    virtual void Resize(const nsIntSize& aSize) {
        mSize = aSize;
        nsIntRegion r(nsIntRect(0, 0, aSize.width, aSize.height));
        BeginUpdate(r);
        EndUpdate();
    }

    /**
     * aSurf - the source surface to update from
     * aRegion - the region in this image to update
     * aFrom - offset in the source to update from
     */
    virtual bool DirectUpdate(gfxASurface *aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom = nsIntPoint(0,0)) = 0;

    virtual void BindTexture(GLenum aTextureUnit) = 0;
    virtual void ReleaseTexture() {};

    class ScopedBindTexture
    {
    public:
        ScopedBindTexture(TextureImage *aTexture, GLenum aTextureUnit) :
          mTexture(aTexture)
        {
            if (mTexture) {
                mTexture->BindTexture(aTextureUnit);
            }
        }

        ~ScopedBindTexture()
        {
            if (mTexture) {
                mTexture->ReleaseTexture();
            }       
        }

    private:
        TextureImage *mTexture;
    };


    /**
     * Returns the shader program type that should be used to render
     * this texture. Only valid after a matching BeginUpdate/EndUpdate
     * pair have been called.
     */
    virtual ShaderProgramType GetShaderProgramType()
    {
         return mShaderType;
    }

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
    GLenum GetWrapMode() const { return mWrapMode; }

    PRBool IsRGB() const { return mIsRGBFormat; }

protected:
    friend class GLContext;

    /**
     * After the ctor, the TextureImage is invalid.  Implementations
     * must allocate resources successfully before returning the new
     * TextureImage from GLContext::CreateTextureImage().  That is,
     * clients must not be given partially-constructed TextureImages.
     */
    TextureImage(const nsIntSize& aSize,
                 GLenum aWrapMode, ContentType aContentType,
                 PRBool aIsRGB = PR_FALSE)
        : mSize(aSize)
        , mWrapMode(aWrapMode)
        , mContentType(aContentType)
        , mIsRGBFormat(aIsRGB)
    {}

    nsIntSize mSize;
    GLenum mWrapMode;
    ContentType mContentType;
    PRPackedBool mIsRGBFormat;
    ShaderProgramType mShaderType;
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
    typedef gfxASurface::gfxImageFormat ImageFormat;
    virtual ~BasicTextureImage();

    BasicTextureImage(GLuint aTexture,
                      const nsIntSize& aSize,
                      GLenum aWrapMode,
                      ContentType aContentType,
                      GLContext* aContext)
        : TextureImage(aSize, aWrapMode, aContentType)
        , mTexture(aTexture)
        , mTextureState(Created)
        , mGLContext(aContext)
        , mUpdateOffset(0, 0)
    {}

    virtual void BindTexture(GLenum aTextureUnit);

    virtual gfxASurface* BeginUpdate(nsIntRegion& aRegion);
    virtual void EndUpdate();
    virtual bool DirectUpdate(gfxASurface* aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom = nsIntPoint(0,0));
    virtual GLuint GetTextureID() { return mTexture; };
    // Returns a surface to draw into
    virtual already_AddRefed<gfxASurface>
      GetSurfaceForUpdate(const gfxIntSize& aSize, ImageFormat aFmt);

    // Call when drawing into the update surface is complete.
    // Returns true if textures should be upload with a relative 
    // offset - See UploadSurfaceToTexture.
    virtual bool FinishedSurfaceUpdate();

    // Call after surface data has been uploaded to a texture.
    virtual void FinishedSurfaceUpload();

    virtual PRBool InUpdate() const { return !!mUpdateSurface; }

    virtual void Resize(const nsIntSize& aSize);
protected:

    GLuint mTexture;
    TextureState mTextureState;
    GLContext* mGLContext;
    nsRefPtr<gfxASurface> mUpdateSurface;
    nsIntRegion mUpdateRegion;

    // The offset into the update surface at which the update rect is located.
    nsIntPoint mUpdateOffset;
};

/**
 * A container class that complements many sub TextureImages into a big TextureImage.
 * Aims to behave just like the real thing.
 */

class TiledTextureImage
    : public TextureImage
{
public:
    TiledTextureImage(GLContext* aGL, nsIntSize aSize,
        TextureImage::ContentType, PRBool aUseNearestFilter = PR_FALSE);
    ~TiledTextureImage();
    void DumpDiv();
    virtual gfxASurface* BeginUpdate(nsIntRegion& aRegion);
    virtual void EndUpdate();
    virtual void Resize(const nsIntSize& aSize);
    virtual void BeginTileIteration();
    virtual PRBool NextTile();
    virtual nsIntRect GetTileRect();
    virtual GLuint GetTextureID() {
        return mImages[mCurrentImage]->GetTextureID();
    };
    virtual bool DirectUpdate(gfxASurface* aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom = nsIntPoint(0,0));
    virtual PRBool InUpdate() const { return mInUpdate; };
    virtual void BindTexture(GLenum);
protected:
    unsigned int mCurrentImage;
    nsTArray< nsRefPtr<TextureImage> > mImages;
    bool mInUpdate;
    nsIntSize mSize;
    unsigned int mTileSize;
    unsigned int mRows, mColumns;
    GLContext* mGL;
    PRBool mUseNearestFilter;
    // A temporary surface to faciliate cross-tile updates.
    nsRefPtr<gfxASurface> mUpdateSurface;
    // The region of update requested
    nsIntRegion mUpdateRegion;
    TextureState mTextureState;
};

struct THEBES_API ContextFormat
{
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
        memset(this, 0, sizeof(*this));
    }

    ContextFormat(const StandardContextFormat cf) {
        memset(this, 0, sizeof(*this));
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
        mVendor(-1),
        mDebugMode(0),
        mCreationFormat(aFormat),
        mSharedContext(aSharedContext),
        mOffscreenTexture(0),
        mFlipped(PR_FALSE),
        mBlitProgram(0),
        mBlitFramebuffer(0),
        mOffscreenFBO(0),
        mOffscreenDepthRB(0),
        mOffscreenStencilRB(0)
#ifdef DEBUG
        , mGLError(LOCAL_GL_NO_ERROR)
#endif
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

    virtual PRBool MakeCurrentImpl(PRBool aForce = PR_FALSE) = 0;

    PRBool MakeCurrent(PRBool aForce = PR_FALSE) {
#ifdef DEBUG
        sCurrentGLContext = this;
#endif
        return MakeCurrentImpl(aForce);
    }

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
        return mSymbols.fUseProgram == nsnull;
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
     * If this GL context has a D3D texture share handle, returns non-null.
     */
    virtual void *GetD3DShareHandle() { return nsnull; }

    /**
     * If this context is double-buffered, returns TRUE.
     */
    virtual PRBool IsDoubleBuffered() { return PR_FALSE; }

    /**
     * If this context is the GLES2 API, returns TRUE.
     * This means that various GLES2 restrictions might be in effect (modulo
     * extensions).
     */
    PRBool IsGLES2() const {
        return mIsGLES2;
    }
    
    /**
     * Returns PR_TRUE if either this is the GLES2 API, or had the GL_ARB_ES2_compatibility extension
     */
    PRBool HasES2Compatibility() {
        return mIsGLES2 || IsExtensionSupported(ARB_ES2_compatibility);
    }

    enum {
        VendorIntel,
        VendorNVIDIA,
        VendorATI,
        VendorQualcomm,
        VendorOther
    };

    int Vendor() const {
        return mVendor;
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

#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
    virtual gfxASurface* GetOffscreenPixmapSurface()
    {
      return 0;
    };
    
    virtual PRBool WaitNative() { return PR_FALSE; }
#endif

    virtual PRBool TextureImageSupportsGetBackingSurface() {
        return PR_FALSE;
    }

    virtual PRBool RenewSurface() { return PR_FALSE; }

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
                       GLenum aWrapMode,
                       PRBool aUseNearestFilter=PR_FALSE);

    /**
     * In EGL we want to use Tiled Texture Images, which we return
     * from CreateTextureImage above.
     * Inside TiledTextureImage we need to create actual images and to
     * prevent infinite recursion we need to differentiate the two
     * functions.
     **/
    virtual already_AddRefed<TextureImage>
    TileGenFunc(const nsIntSize& aSize,
                TextureImage::ContentType aContentType,
                PRBool aUseNearestFilter = PR_FALSE)
    {
        return nsnull;
    };

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
    void THEBES_API ReadPixelsIntoImageSurface(GLint aX, GLint aY,
                                    GLsizei aWidth, GLsizei aHeight,
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
     * Creates a RGB/RGBA texture (or uses one provided) and uploads the surface
     * contents to it within aSrcRect.
     *
     * aSrcRect.x/y will be uploaded to 0/0 in the texture, and the size
     * of the texture with be aSrcRect.width/height.
     *
     * If an existing texture is passed through aTexture, it is assumed it
     * has already been initialised with glTexImage2D (or this function),
     * and that its size is equal to or greater than aSrcRect + aDstPoint.
     * You can alternatively set the overwrite flag to true and have a new
     * texture memory block allocated.
     *
     * The aDstPoint parameter is ignored if no texture was provided
     * or aOverwrite is true.
     *
     * \param aSurface Surface to upload. 
     * \param aDstRegion Region of texture to upload to.
     * \param aTexture Texture to use, or 0 to have one created for you.
     * \param aOverwrite Over an existing texture with a new one.
     * \param aSrcPoint Offset into aSrc where the region's bound's 
     *  TopLeft() sits.
     * \param aPixelBuffer Pass true to upload texture data with an
     *  offset from the base data (generally for pixel buffer objects), 
     *  otherwise textures are upload with an absolute pointer to the data.
     * \return Shader program needed to render this texture.
     */
    ShaderProgramType UploadSurfaceToTexture(gfxASurface *aSurface, 
                                             const nsIntRegion& aDstRegion,
                                             GLuint& aTexture,
                                             bool aOverwrite = false,
                                             const nsIntPoint& aSrcPoint = nsIntPoint(0, 0),
                                             bool aPixelBuffer = PR_FALSE);

    
    void TexImage2D(GLenum target, GLint level, GLint internalformat, 
                    GLsizei width, GLsizei height, GLsizei stride,
                    GLint pixelsize, GLint border, GLenum format, 
                    GLenum type, const GLvoid *pixels);


    void TexSubImage2D(GLenum target, GLint level, 
                       GLint xoffset, GLint yoffset, 
                       GLsizei width, GLsizei height, GLsizei stride,
                       GLint pixelsize, GLenum format, 
                       GLenum type, const GLvoid* pixels);

    /** Helper for DecomposeIntoNoRepeatTriangles
     */
    struct RectTriangles {
        RectTriangles() { }

        void addRect(GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1,
                     GLfloat tx0, GLfloat ty0, GLfloat tx1, GLfloat ty1);

        /**
         * these return a float pointer to the start of each array respectively.
         * Use it for glVertexAttribPointer calls.
         * We can return NULL if we choose to use Vertex Buffer Objects here.
         */
        float* vertexPointer() {
            return &vertexCoords[0].x;
        };

        float* texCoordPointer() {
            return &texCoords[0].u;
        };

        unsigned int elements() {
            return vertexCoords.Length();
        };

        typedef struct { GLfloat x,y; } vert_coord;
        typedef struct { GLfloat u,v; } tex_coord;
    private:
        // default is 4 rectangles, each made up of 2 triangles (3 coord vertices each)
        nsAutoTArray<vert_coord, 6> vertexCoords;
        nsAutoTArray<tex_coord, 6>  texCoords;
    };

    /**
     * Decompose drawing the possibly-wrapped aTexCoordRect rectangle
     * of a texture of aTexSize into one or more rectangles (represented
     * as 2 triangles) and associated tex coordinates, such that
     * we don't have to use the REPEAT wrap mode.
     *
     * The resulting triangle vertex coordinates will be in the space of
     * (0.0, 0.0) to (1.0, 1.0) -- transform the coordinates appropriately
     * if you need a different space.
     *
     * The resulting vertex coordinates should be drawn using GL_TRIANGLES,
     * and rects.numRects * 3 * 6
     */
    static void DecomposeIntoNoRepeatTriangles(const nsIntRect& aTexCoordRect,
                                               const nsIntSize& aTexSize,
                                               RectTriangles& aRects);

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
        APPLE_client_storage,
        ARB_texture_non_power_of_two,
        ARB_pixel_buffer_object,
        ARB_ES2_compatibility,
        OES_texture_float,
        ARB_texture_float,
        Extensions_Max
    };

    PRBool IsExtensionSupported(GLExtensions aKnownExtension) {
        return mAvailableExtensions[aKnownExtension];
    }

    // for unknown extensions
    PRBool IsExtensionSupported(const char *extension);

    // Shared code for GL extensions and GLX extensions.
    static PRBool ListHasExtension(const GLubyte *extensions,
                                   const char *extension);

    GLint GetMaxTextureSize() { return mMaxTextureSize; }
    void SetFlipped(PRBool aFlipped) { mFlipped = aFlipped; }

    // this should just be a std::bitset, but that ended up breaking
    // MacOS X builds; see bug 584919.  We can replace this with one
    // later on.  This is handy to use in WebGL contexts as well,
    // so making it public.
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

protected:
    PRPackedBool mInitialized;
    PRPackedBool mIsOffscreen;
    PRPackedBool mIsGLES2;
    PRPackedBool mIsGlobalSharedContext;

    PRInt32 mVendor;

    enum {
        DebugEnabled = 1 << 0,
        DebugTrace = 1 << 1,
        DebugAbortOnError = 1 << 2
    };

    PRUint32 mDebugMode;

    ContextFormat mCreationFormat;
    nsRefPtr<GLContext> mSharedContext;

    GLContextSymbols mSymbols;

#ifdef DEBUG
    // this should be thread-local, but that is slightly annoying to implement because on Mac
    // we don't have any __thread-like keyword. So for now, MOZ_GL_DEBUG assumes (and asserts)
    // that only the main thread is doing OpenGL calls.
    static THEBES_API GLContext* sCurrentGLContext;
#endif

    void UpdateActualFormat();
    ContextFormat mActualFormat;

    gfxIntSize mOffscreenSize;
    gfxIntSize mOffscreenActualSize;
    GLuint mOffscreenTexture;
    PRBool mFlipped;

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

    virtual already_AddRefed<TextureImage>
    CreateBasicTextureImage(GLuint aTexture,
                            const nsIntSize& aSize,
                            GLenum aWrapMode,
                            TextureImage::ContentType aContentType,
                            GLContext* aContext)
    {
        nsRefPtr<BasicTextureImage> teximage(
            new BasicTextureImage(aTexture, aSize, aWrapMode, aContentType, aContext));
        return teximage.forget();
    }

    bool IsOffscreenSizeAllowed(const gfxIntSize& aSize) const {
        PRInt32 biggerDimension = NS_MAX(aSize.width, aSize.height);
        PRInt32 maxAllowed = NS_MIN(mMaxRenderbufferSize, mMaxTextureSize);
        return biggerDimension <= maxAllowed;
    }

protected:
    nsTArray<nsIntRect> mViewportStack;
    nsTArray<nsIntRect> mScissorStack;

    GLint mMaxTextureSize;
    GLint mMaxRenderbufferSize;

public:
 
    /** \returns the first GL error, and guarantees that all GL error flags are cleared,
      * i.e. that a subsequent GetError call will return NO_ERROR
      */
    GLenum GetAndClearError() {
        // the first error is what we want to return
        GLenum error = fGetError();
        
        if (error) {
            // clear all pending errors
            while(fGetError()) {}
        }
        
        return error;
    }

#ifdef DEBUG

#ifndef MOZ_FUNCTION_NAME
# ifdef __GNUC__
#  define MOZ_FUNCTION_NAME __PRETTY_FUNCTION__
# elif defined(_MSC_VER)
#  define MOZ_FUNCTION_NAME __FUNCTION__
# else
#  define MOZ_FUNCTION_NAME __func__  // defined in C99, supported in various C++ compilers. Just raw function name.
# endif
#endif

protected:
    GLenum mGLError;

public:

    void BeforeGLCall(const char* glFunction) {
        if (mDebugMode) {
            // since the static member variable sCurrentGLContext is not thread-local as it should,
            // we have to assert that we're in the main thread. Note that sCurrentGLContext is only used
            // for the OpenGL debug mode.
            if (!NS_IsMainThread()) {
                NS_ERROR("OpenGL call from non-main thread. While this is fine in itself, "
                         "the OpenGL debug mode, which is currently enabled, doesn't support this. "
                         "It needs to be patched by making GLContext::sCurrentGLContext be thread-local.\n");
                NS_ABORT();
            }
            if (mDebugMode & DebugTrace)
                printf_stderr("[gl:%p] > %s\n", this, glFunction);
            if (this != sCurrentGLContext) {
                printf_stderr("Fatal: %s called on non-current context %p. "
                              "The current context for this thread is %p.\n",
                               glFunction, this, sCurrentGLContext);
                NS_ABORT();
            }
        }
    }

    void AfterGLCall(const char* glFunction) {
        if (mDebugMode) {
            // calling fFinish() immediately after every GL call makes sure that if this GL command crashes,
            // the stack trace will actually point to it. Otherwise, OpenGL being an asynchronous API, stack traces
            // tend to be meaningless
            mSymbols.fFinish();
            mGLError = mSymbols.fGetError();
            if (mDebugMode & DebugTrace)
                printf_stderr("[gl:%p] < %s [0x%04x]\n", this, glFunction, mGLError);
            if (mGLError != LOCAL_GL_NO_ERROR) {
                printf_stderr("GL ERROR: %s generated GL error %s(0x%04x)\n", 
                              glFunction,
                              GLErrorToString(mGLError),
                              mGLError);
                if (mDebugMode & DebugAbortOnError)
                    NS_ABORT();
            }
        }
    }

    const char* GLErrorToString(GLenum aError)
    {
        switch (aError) {
            case LOCAL_GL_INVALID_ENUM:
                return "GL_INVALID_ENUM";
            case LOCAL_GL_INVALID_VALUE:
                return "GL_INVALID_VALUE";
            case LOCAL_GL_INVALID_OPERATION:
                return "GL_INVALID_OPERATION";
            case LOCAL_GL_STACK_OVERFLOW:
                return "GL_STACK_OVERFLOW";
            case LOCAL_GL_STACK_UNDERFLOW:
                return "GL_STACK_UNDERFLOW";
            case LOCAL_GL_OUT_OF_MEMORY:
                return "GL_OUT_OF_MEMORY";
            case LOCAL_GL_TABLE_TOO_LARGE:
                return "GL_TABLE_TOO_LARGE";
            case LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION:
                return "GL_INVALID_FRAMEBUFFER_OPERATION";
            default:
                return "";
        }
     }

#define BEFORE_GL_CALL do {                     \
    BeforeGLCall(MOZ_FUNCTION_NAME);            \
} while (0)
    
#define AFTER_GL_CALL do {                      \
    AfterGLCall(MOZ_FUNCTION_NAME);             \
} while (0)

#else

#define BEFORE_GL_CALL do { } while (0)
#define AFTER_GL_CALL do { } while (0)

#endif

    /*** In GL debug mode, we completely override glGetError ***/

    GLenum fGetError() {
#ifdef DEBUG
        // debug mode ends up eating the error in AFTER_GL_CALL
        if (mDebugMode) {
            GLenum err = mGLError;
            mGLError = LOCAL_GL_NO_ERROR;
            return err;
        }
#endif

        return mSymbols.fGetError();
    }


    /*** Scissor functions ***/

protected:

    GLint FixYValue(GLint y, GLint height)
    {
        return mFlipped ? ViewportRect().height - (height + y) : y;
    }

    // only does the glScissor call, no ScissorRect business
    void raw_fScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
        BEFORE_GL_CALL;
        // GL's coordinate system is flipped compared to ours (in the Y axis),
        // so we may need to flip our rectangle.
        mSymbols.fScissor(x, 
                          FixYValue(y, height),
                          width, 
                          height);
        AFTER_GL_CALL;
    }

public:

    // but let GL-using code use that instead, updating the ScissorRect
    void fScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
        ScissorRect().SetRect(x, y, width, height);
        raw_fScissor(x, y, width, height);
    }

    nsIntRect& ScissorRect() {
        return mScissorStack[mScissorStack.Length()-1];
    }

    void PushScissorRect() {
        nsIntRect copy(ScissorRect());
        mScissorStack.AppendElement(copy);
    }

    void PushScissorRect(const nsIntRect& aRect) {
        mScissorStack.AppendElement(aRect);
        raw_fScissor(aRect.x, aRect.y, aRect.width, aRect.height);
    }

    void PopScissorRect() {
        if (mScissorStack.Length() < 2) {
            NS_WARNING("PopScissorRect with Length < 2!");
            return;
        }

        nsIntRect thisRect = ScissorRect();
        mScissorStack.TruncateLength(mScissorStack.Length() - 1);
        if (!thisRect.IsEqualInterior(ScissorRect())) {
            raw_fScissor(ScissorRect().x, ScissorRect().y,
                              ScissorRect().width, ScissorRect().height);
        }
    }

    /*** Viewport functions ***/

protected:

    // only does the glViewport call, no ViewportRect business
    void raw_fViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
        BEFORE_GL_CALL;
        // XXX: Flipping should really happen using the destination height, but
        // we use viewport instead and assume viewport size matches the
        // destination. If we ever try use partial viewports for layers we need
        // to fix this, and remove the assertion.
        NS_ASSERTION(!mFlipped || (x == 0 && y == 0), "TODO: Need to flip the viewport rect"); 
        mSymbols.fViewport(x, y, width, height);
        AFTER_GL_CALL;
    }

public:

    void fViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
        ViewportRect().SetRect(x, y, width, height);
        raw_fViewport(x, y, width, height);
    }

    nsIntRect& ViewportRect() {
        return mViewportStack[mViewportStack.Length()-1];
    }

    void PushViewportRect() {
        nsIntRect copy(ViewportRect());
        mViewportStack.AppendElement(copy);
    }

    void PushViewportRect(const nsIntRect& aRect) {
        mViewportStack.AppendElement(aRect);
        raw_fViewport(aRect.x, aRect.y, aRect.width, aRect.height);
    }

    void PopViewportRect() {
        if (mViewportStack.Length() < 2) {
            NS_WARNING("PopViewportRect with Length < 2!");
            return;
        }

        nsIntRect thisRect = ViewportRect();
        mViewportStack.TruncateLength(mViewportStack.Length() - 1);
        if (!thisRect.IsEqualInterior(ViewportRect())) {
            raw_fViewport(ViewportRect().x, ViewportRect().y,
                          ViewportRect().width, ViewportRect().height);
        }
    }

    /*** other GL functions ***/

    void fActiveTexture(GLenum texture) {
        BEFORE_GL_CALL;
        mSymbols.fActiveTexture(texture);
        AFTER_GL_CALL;
    }

    void fAttachShader(GLuint program, GLuint shader) {
        BEFORE_GL_CALL;
        mSymbols.fAttachShader(program, shader);
        AFTER_GL_CALL;
    }

    void fBindAttribLocation(GLuint program, GLuint index, const GLchar* name) {
        BEFORE_GL_CALL;
        mSymbols.fBindAttribLocation(program, index, name);
        AFTER_GL_CALL;
    }

    void fBindBuffer(GLenum target, GLuint buffer) {
        BEFORE_GL_CALL;
        mSymbols.fBindBuffer(target, buffer);
        AFTER_GL_CALL;
    }

    void fBindTexture(GLenum target, GLuint texture) {
        BEFORE_GL_CALL;
        mSymbols.fBindTexture(target, texture);
        AFTER_GL_CALL;
    }

    void fBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
        BEFORE_GL_CALL;
        mSymbols.fBlendColor(red, green, blue, alpha);
        AFTER_GL_CALL;
    }

    void fBlendEquation(GLenum mode) {
        BEFORE_GL_CALL;
        mSymbols.fBlendEquation(mode);
        AFTER_GL_CALL;
    }

    void fBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
        BEFORE_GL_CALL;
        mSymbols.fBlendEquationSeparate(modeRGB, modeAlpha);
        AFTER_GL_CALL;
    }

    void fBlendFunc(GLenum sfactor, GLenum dfactor) {
        BEFORE_GL_CALL;
        mSymbols.fBlendFunc(sfactor, dfactor);
        AFTER_GL_CALL;
    }

    void fBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
        BEFORE_GL_CALL;
        mSymbols.fBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
        AFTER_GL_CALL;
    }

    void fBufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage) {
        BEFORE_GL_CALL;
        mSymbols.fBufferData(target, size, data, usage);
        AFTER_GL_CALL;
    }

    void fBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data) {
        BEFORE_GL_CALL;
        mSymbols.fBufferSubData(target, offset, size, data);
        AFTER_GL_CALL;
    }

    void fClear(GLbitfield mask) {
        BEFORE_GL_CALL;
        mSymbols.fClear(mask);
        AFTER_GL_CALL;
    }

    void fClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) {
        BEFORE_GL_CALL;
        mSymbols.fClearColor(r, g, b, a);
        AFTER_GL_CALL;
    }

    void fClearStencil(GLint s) {
        BEFORE_GL_CALL;
        mSymbols.fClearStencil(s);
        AFTER_GL_CALL;
    }

    void fColorMask(realGLboolean red, realGLboolean green, realGLboolean blue, realGLboolean alpha) {
        BEFORE_GL_CALL;
        mSymbols.fColorMask(red, green, blue, alpha);
        AFTER_GL_CALL;
    }

    void fCullFace(GLenum mode) {
        BEFORE_GL_CALL;
        mSymbols.fCullFace(mode);
        AFTER_GL_CALL;
    }

    void fDetachShader(GLuint program, GLuint shader) {
        BEFORE_GL_CALL;
        mSymbols.fDetachShader(program, shader);
        AFTER_GL_CALL;
    }

    void fDepthFunc(GLenum func) {
        BEFORE_GL_CALL;
        mSymbols.fDepthFunc(func);
        AFTER_GL_CALL;
    }

    void fDepthMask(realGLboolean flag) {
        BEFORE_GL_CALL;
        mSymbols.fDepthMask(flag);
        AFTER_GL_CALL;
    }

    void fDisable(GLenum capability) {
        BEFORE_GL_CALL;
        mSymbols.fDisable(capability);
        AFTER_GL_CALL;
    }

    void fDisableVertexAttribArray(GLuint index) {
        BEFORE_GL_CALL;
        mSymbols.fDisableVertexAttribArray(index);
        AFTER_GL_CALL;
    }

    void fDrawArrays(GLenum mode, GLint first, GLsizei count) {
        BEFORE_GL_CALL;
        mSymbols.fDrawArrays(mode, first, count);
        AFTER_GL_CALL;
    }

    void fDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
        BEFORE_GL_CALL;
        mSymbols.fDrawElements(mode, count, type, indices);
        AFTER_GL_CALL;
    }

    void fEnable(GLenum capability) {
        BEFORE_GL_CALL;
        mSymbols.fEnable(capability);
        AFTER_GL_CALL;
    }

    void fEnableVertexAttribArray(GLuint index) {
        BEFORE_GL_CALL;
        mSymbols.fEnableVertexAttribArray(index);
        AFTER_GL_CALL;
    }

    void fFinish() {
        BEFORE_GL_CALL;
        mSymbols.fFinish();
        AFTER_GL_CALL;
    }

    void fFlush() {
        BEFORE_GL_CALL;
        mSymbols.fFlush();
        AFTER_GL_CALL;
    }

    void fFrontFace(GLenum face) {
        BEFORE_GL_CALL;
        mSymbols.fFrontFace(face);
        AFTER_GL_CALL;
    }

    void fGetActiveAttrib(GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name) {
        BEFORE_GL_CALL;
        mSymbols.fGetActiveAttrib(program, index, maxLength, length, size, type, name);
        AFTER_GL_CALL;
    }

    void fGetActiveUniform(GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name) {
        BEFORE_GL_CALL;
        mSymbols.fGetActiveUniform(program, index, maxLength, length, size, type, name);
        AFTER_GL_CALL;
    }

    void fGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders) {
        BEFORE_GL_CALL;
        mSymbols.fGetAttachedShaders(program, maxCount, count, shaders);
        AFTER_GL_CALL;
    }

    GLint fGetAttribLocation (GLuint program, const GLchar* name) {
        BEFORE_GL_CALL;
        GLint retval = mSymbols.fGetAttribLocation(program, name);
        AFTER_GL_CALL;
        return retval;
    }

    void fGetIntegerv(GLenum pname, GLint *params) {
        BEFORE_GL_CALL;
        mSymbols.fGetIntegerv(pname, params);
        AFTER_GL_CALL;
    }

    void fGetFloatv(GLenum pname, GLfloat *params) {
        BEFORE_GL_CALL;
        mSymbols.fGetFloatv(pname, params);
        AFTER_GL_CALL;
    }

    void fGetBooleanv(GLenum pname, realGLboolean *params) {
        BEFORE_GL_CALL;
        mSymbols.fGetBooleanv(pname, params);
        AFTER_GL_CALL;
    }

    void fGetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
        BEFORE_GL_CALL;
        mSymbols.fGetBufferParameteriv(target, pname, params);
        AFTER_GL_CALL;
    }

    void fGenerateMipmap(GLenum target) {
        BEFORE_GL_CALL;
        mSymbols.fGenerateMipmap(target);
        AFTER_GL_CALL;
    }

    void fGetProgramiv(GLuint program, GLenum pname, GLint* param) {
        BEFORE_GL_CALL;
        mSymbols.fGetProgramiv(program, pname, param);
        AFTER_GL_CALL;
    }

    void fGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
        BEFORE_GL_CALL;
        mSymbols.fGetProgramInfoLog(program, bufSize, length, infoLog);
        AFTER_GL_CALL;
    }

    void fTexParameteri(GLenum target, GLenum pname, GLint param) {
        BEFORE_GL_CALL;
        mSymbols.fTexParameteri(target, pname, param);
        AFTER_GL_CALL;
    }

    void fTexParameterf(GLenum target, GLenum pname, GLfloat param) {
        BEFORE_GL_CALL;
        mSymbols.fTexParameterf(target, pname, param);
        AFTER_GL_CALL;
    }

    const GLubyte* fGetString(GLenum name) {
        BEFORE_GL_CALL;
        const GLubyte *result = mSymbols.fGetString(name);
        AFTER_GL_CALL;
        return result;
    }

    void fGetTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) {
        BEFORE_GL_CALL;
        mSymbols.fGetTexParameterfv(target, pname, params);
        AFTER_GL_CALL;
    }

    void fGetTexParameteriv(GLenum target, GLenum pname, const GLint *params) {
        BEFORE_GL_CALL;
        mSymbols.fGetTexParameteriv(target, pname, params);
        AFTER_GL_CALL;
    }

    void fGetUniformfv(GLuint program, GLint location, GLfloat* params) {
        BEFORE_GL_CALL;
        mSymbols.fGetUniformfv(program, location, params);
        AFTER_GL_CALL;
    }

    void fGetUniformiv(GLuint program, GLint location, GLint* params) {
        BEFORE_GL_CALL;
        mSymbols.fGetUniformiv(program, location, params);
        AFTER_GL_CALL;
    }

    GLint fGetUniformLocation (GLint programObj, const GLchar* name) {
        BEFORE_GL_CALL;
        GLint retval = mSymbols.fGetUniformLocation(programObj, name);
        AFTER_GL_CALL;
        return retval;
    }

    void fGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* retval) {
        BEFORE_GL_CALL;
        mSymbols.fGetVertexAttribfv(index, pname, retval);
        AFTER_GL_CALL;
    }

    void fGetVertexAttribiv(GLuint index, GLenum pname, GLint* retval) {
        BEFORE_GL_CALL;
        mSymbols.fGetVertexAttribiv(index, pname, retval);
        AFTER_GL_CALL;
    }

    void fHint(GLenum target, GLenum mode) {
        BEFORE_GL_CALL;
        mSymbols.fHint(target, mode);
        AFTER_GL_CALL;
    }

    realGLboolean fIsBuffer(GLuint buffer) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsBuffer(buffer);
        AFTER_GL_CALL;
        return retval;
    }

    realGLboolean fIsEnabled (GLenum capability) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsEnabled(capability);
        AFTER_GL_CALL;
        return retval;
    }

    realGLboolean fIsProgram (GLuint program) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsProgram(program);
        AFTER_GL_CALL;
        return retval;
    }

    realGLboolean fIsShader (GLuint shader) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsShader(shader);
        AFTER_GL_CALL;
        return retval;
    }

    realGLboolean fIsTexture (GLuint texture) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsTexture(texture);
        AFTER_GL_CALL;
        return retval;
    }

    void fLineWidth(GLfloat width) {
        BEFORE_GL_CALL;
        mSymbols.fLineWidth(width);
        AFTER_GL_CALL;
    }

    void fLinkProgram(GLuint program) {
        BEFORE_GL_CALL;
        mSymbols.fLinkProgram(program);
        AFTER_GL_CALL;
    }

    void fPixelStorei(GLenum pname, GLint param) {
        BEFORE_GL_CALL;
        mSymbols.fPixelStorei(pname, param);
        AFTER_GL_CALL;
    }

    void fPolygonOffset(GLfloat factor, GLfloat bias) {
        BEFORE_GL_CALL;
        mSymbols.fPolygonOffset(factor, bias);
        AFTER_GL_CALL;
    }

    void fReadBuffer(GLenum mode) {
        BEFORE_GL_CALL;
        mSymbols.fReadBuffer(mode);
        AFTER_GL_CALL;
    }

    void fReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) {
        BEFORE_GL_CALL;
        mSymbols.fReadPixels(x, y, width, height, format, type, pixels);
        AFTER_GL_CALL;
    }

    void fSampleCoverage(GLclampf value, realGLboolean invert) {
        BEFORE_GL_CALL;
        mSymbols.fSampleCoverage(value, invert);
        AFTER_GL_CALL;
    }

    void fStencilFunc(GLenum func, GLint ref, GLuint mask) {
        BEFORE_GL_CALL;
        mSymbols.fStencilFunc(func, ref, mask);
        AFTER_GL_CALL;
    }

    void fStencilFuncSeparate(GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask) {
        BEFORE_GL_CALL;
        mSymbols.fStencilFuncSeparate(frontfunc, backfunc, ref, mask);
        AFTER_GL_CALL;
    }

    void fStencilMask(GLuint mask) {
        BEFORE_GL_CALL;
        mSymbols.fStencilMask(mask);
        AFTER_GL_CALL;
    }

    void fStencilMaskSeparate(GLenum face, GLuint mask) {
        BEFORE_GL_CALL;
        mSymbols.fStencilMaskSeparate(face, mask);
        AFTER_GL_CALL;
    }

    void fStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
        BEFORE_GL_CALL;
        mSymbols.fStencilOp(fail, zfail, zpass);
        AFTER_GL_CALL;
    }

    void fStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
        BEFORE_GL_CALL;
        mSymbols.fStencilOpSeparate(face, sfail, dpfail, dppass);
        AFTER_GL_CALL;
    }

    void fTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {
        BEFORE_GL_CALL;
        mSymbols.fTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
        AFTER_GL_CALL;
    }

    void fTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels) {
        BEFORE_GL_CALL;
        mSymbols.fTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
        AFTER_GL_CALL;
    }

    void fUniform1f(GLint location, GLfloat v0) {
        BEFORE_GL_CALL;
        mSymbols.fUniform1f(location, v0);
        AFTER_GL_CALL;
    }

    void fUniform1fv(GLint location, GLsizei count, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform1fv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform1i(GLint location, GLint v0) {
        BEFORE_GL_CALL;
        mSymbols.fUniform1i(location, v0);
        AFTER_GL_CALL;
    }

    void fUniform1iv(GLint location, GLsizei count, const GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform1iv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform2f(GLint location, GLfloat v0, GLfloat v1) {
        BEFORE_GL_CALL;
        mSymbols.fUniform2f(location, v0, v1);
        AFTER_GL_CALL;
    }

    void fUniform2fv(GLint location, GLsizei count, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform2fv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform2i(GLint location, GLint v0, GLint v1) {
        BEFORE_GL_CALL;
        mSymbols.fUniform2i(location, v0, v1);
        AFTER_GL_CALL;
    }

    void fUniform2iv(GLint location, GLsizei count, const GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform2iv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
        BEFORE_GL_CALL;
        mSymbols.fUniform3f(location, v0, v1, v2);
        AFTER_GL_CALL;
    }

    void fUniform3fv(GLint location, GLsizei count, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform3fv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
        BEFORE_GL_CALL;
        mSymbols.fUniform3i(location, v0, v1, v2);
        AFTER_GL_CALL;
    }

    void fUniform3iv(GLint location, GLsizei count, const GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform3iv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
        BEFORE_GL_CALL;
        mSymbols.fUniform4f(location, v0, v1, v2, v3);
        AFTER_GL_CALL;
    }

    void fUniform4fv(GLint location, GLsizei count, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform4fv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
        BEFORE_GL_CALL;
        mSymbols.fUniform4i(location, v0, v1, v2, v3);
        AFTER_GL_CALL;
    }

    void fUniform4iv(GLint location, GLsizei count, const GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniform4iv(location, count, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix2fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniformMatrix2fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix3fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniformMatrix3fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUniformMatrix4fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value) {
        BEFORE_GL_CALL;
        mSymbols.fUniformMatrix4fv(location, count, transpose, value);
        AFTER_GL_CALL;
    }

    void fUseProgram(GLuint program) {
        BEFORE_GL_CALL;
        mSymbols.fUseProgram(program);
        AFTER_GL_CALL;
    }

    void fValidateProgram(GLuint program) {
        BEFORE_GL_CALL;
        mSymbols.fValidateProgram(program);
        AFTER_GL_CALL;
    }

    void fVertexAttribPointer(GLuint index, GLint size, GLenum type, realGLboolean normalized, GLsizei stride, const GLvoid* pointer) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttribPointer(index, size, type, normalized, stride, pointer);
        AFTER_GL_CALL;
    }

    void fVertexAttrib1f(GLuint index, GLfloat x) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib1f(index, x);
        AFTER_GL_CALL;
    }

    void fVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib2f(index, x, y);
        AFTER_GL_CALL;
    }

    void fVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib3f(index, x, y, z);
        AFTER_GL_CALL;
    }

    void fVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib4f(index, x, y, z, w);
        AFTER_GL_CALL;
    }

    void fVertexAttrib1fv(GLuint index, const GLfloat* v) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib1fv(index, v);
        AFTER_GL_CALL;
    }

    void fVertexAttrib2fv(GLuint index, const GLfloat* v) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib2fv(index, v);
        AFTER_GL_CALL;
    }

    void fVertexAttrib3fv(GLuint index, const GLfloat* v) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib3fv(index, v);
        AFTER_GL_CALL;
    }

    void fVertexAttrib4fv(GLuint index, const GLfloat* v) {
        BEFORE_GL_CALL;
        mSymbols.fVertexAttrib4fv(index, v);
        AFTER_GL_CALL;
    }

    void fCompileShader(GLuint shader) {
        BEFORE_GL_CALL;
        mSymbols.fCompileShader(shader);
        AFTER_GL_CALL;
    }

    void fCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
        BEFORE_GL_CALL;
        mSymbols.fCopyTexImage2D(target, level, internalformat, 
                                 x, FixYValue(y, height),
                                 width, height, border);
        AFTER_GL_CALL;
    }

    void fCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
        BEFORE_GL_CALL;
        mSymbols.fCopyTexSubImage2D(target, level, xoffset, yoffset, 
                                    x, FixYValue(y, height),
                                    width, height);
        AFTER_GL_CALL;
    }

    void fGetShaderiv(GLuint shader, GLenum pname, GLint* param) {
        BEFORE_GL_CALL;
        mSymbols.fGetShaderiv(shader, pname, param);
        AFTER_GL_CALL;
    }

    void fGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
        BEFORE_GL_CALL;
        mSymbols.fGetShaderInfoLog(shader, bufSize, length, infoLog);
        AFTER_GL_CALL;
    }

    void fGetShaderSource(GLint obj, GLsizei maxLength, GLsizei* length, GLchar* source) {
        BEFORE_GL_CALL;
        mSymbols.fGetShaderSource(obj, maxLength, length, source);
        AFTER_GL_CALL;
    }

    void fShaderSource(GLuint shader, GLsizei count, const GLchar** strings, const GLint* lengths) {
        BEFORE_GL_CALL;
        mSymbols.fShaderSource(shader, count, strings, lengths);
        AFTER_GL_CALL;
    }

    void fBindFramebuffer(GLenum target, GLuint framebuffer) {
        BEFORE_GL_CALL;
        mSymbols.fBindFramebuffer(target, framebuffer);
        AFTER_GL_CALL;
    }

    void fBindRenderbuffer(GLenum target, GLuint renderbuffer) {
        BEFORE_GL_CALL;
        mSymbols.fBindRenderbuffer(target, renderbuffer);
        AFTER_GL_CALL;
    }

    GLenum fCheckFramebufferStatus (GLenum target) {
        BEFORE_GL_CALL;
        GLenum retval = mSymbols.fCheckFramebufferStatus(target);
        AFTER_GL_CALL;
        return retval;
    }

    void fFramebufferRenderbuffer(GLenum target, GLenum attachmentPoint, GLenum renderbufferTarget, GLuint renderbuffer) {
        BEFORE_GL_CALL;
        mSymbols.fFramebufferRenderbuffer(target, attachmentPoint, renderbufferTarget, renderbuffer);
        AFTER_GL_CALL;
    }

    void fFramebufferTexture2D(GLenum target, GLenum attachmentPoint, GLenum textureTarget, GLuint texture, GLint level) {
        BEFORE_GL_CALL;
        mSymbols.fFramebufferTexture2D(target, attachmentPoint, textureTarget, texture, level);
        AFTER_GL_CALL;
    }

    void fGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fGetFramebufferAttachmentParameteriv(target, attachment, pname, value);
        AFTER_GL_CALL;
    }

    void fGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* value) {
        BEFORE_GL_CALL;
        mSymbols.fGetRenderbufferParameteriv(target, pname, value);
        AFTER_GL_CALL;
    }

    realGLboolean fIsFramebuffer (GLuint framebuffer) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsFramebuffer(framebuffer);
        AFTER_GL_CALL;
        return retval;
    }

    realGLboolean fIsRenderbuffer (GLuint renderbuffer) {
        BEFORE_GL_CALL;
        realGLboolean retval = mSymbols.fIsRenderbuffer(renderbuffer);
        AFTER_GL_CALL;
        return retval;
    }

    void fRenderbufferStorage(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height) {
        BEFORE_GL_CALL;
        mSymbols.fRenderbufferStorage(target, internalFormat, width, height);
        AFTER_GL_CALL;
    }

    void fDepthRange(GLclampf a, GLclampf b) {
        BEFORE_GL_CALL;
        if (mIsGLES2) {
            mSymbols.fDepthRangef(a, b);
        } else {
            mSymbols.fDepthRange(a, b);
        }
        AFTER_GL_CALL;
    }

    void fClearDepth(GLclampf v) {
        BEFORE_GL_CALL;
        if (mIsGLES2) {
            mSymbols.fClearDepthf(v);
        } else {
            mSymbols.fClearDepth(v);
        }
        AFTER_GL_CALL;
    }

    void* fMapBuffer(GLenum target, GLenum access) {
        BEFORE_GL_CALL;
        void *ret = mSymbols.fMapBuffer(target, access);
        AFTER_GL_CALL;
        return ret;
    }

    realGLboolean fUnmapBuffer(GLenum target) {
        BEFORE_GL_CALL;
        realGLboolean ret = mSymbols.fUnmapBuffer(target);
        AFTER_GL_CALL;
        return ret;
    }


#ifdef DEBUG
     GLContext *TrackingContext() {
         GLContext *tip = this;
         while (tip->mSharedContext)
             tip = tip->mSharedContext;
         return tip;
     }

#define TRACKING_CONTEXT(a) do { TrackingContext()->a; } while (0)
#else
#define TRACKING_CONTEXT(a) do {} while (0)
#endif

     GLuint GLAPIENTRY fCreateProgram() {
         BEFORE_GL_CALL;
         GLuint ret = mSymbols.fCreateProgram();
         AFTER_GL_CALL;
         TRACKING_CONTEXT(CreatedProgram(this, ret));
         return ret;
     }

     GLuint GLAPIENTRY fCreateShader(GLenum t) {
         BEFORE_GL_CALL;
         GLuint ret = mSymbols.fCreateShader(t);
         AFTER_GL_CALL;
         TRACKING_CONTEXT(CreatedShader(this, ret));
         return ret;
     }

     void GLAPIENTRY fGenBuffers(GLsizei n, GLuint* names) {
         BEFORE_GL_CALL;
         mSymbols.fGenBuffers(n, names);
         AFTER_GL_CALL;
         TRACKING_CONTEXT(CreatedBuffers(this, n, names));
     }

     void GLAPIENTRY fGenTextures(GLsizei n, GLuint* names) {
         BEFORE_GL_CALL;
         mSymbols.fGenTextures(n, names);
         AFTER_GL_CALL;
         TRACKING_CONTEXT(CreatedTextures(this, n, names));
     }

     void GLAPIENTRY fGenFramebuffers(GLsizei n, GLuint* names) {
         BEFORE_GL_CALL;
         mSymbols.fGenFramebuffers(n, names);
         AFTER_GL_CALL;
         TRACKING_CONTEXT(CreatedFramebuffers(this, n, names));
     }

     void GLAPIENTRY fGenRenderbuffers(GLsizei n, GLuint* names) {
         BEFORE_GL_CALL;
         mSymbols.fGenRenderbuffers(n, names);
         AFTER_GL_CALL;
         TRACKING_CONTEXT(CreatedRenderbuffers(this, n, names));
     }

     void GLAPIENTRY fDeleteProgram(GLuint program) {
         BEFORE_GL_CALL;
         mSymbols.fDeleteProgram(program);
         AFTER_GL_CALL;
         TRACKING_CONTEXT(DeletedProgram(this, program));
     }

     void GLAPIENTRY fDeleteShader(GLuint shader) {
         BEFORE_GL_CALL;
         mSymbols.fDeleteShader(shader);
         AFTER_GL_CALL;
         TRACKING_CONTEXT(DeletedShader(this, shader));
     }

     void GLAPIENTRY fDeleteBuffers(GLsizei n, GLuint *names) {
         BEFORE_GL_CALL;
         mSymbols.fDeleteBuffers(n, names);
         AFTER_GL_CALL;
         TRACKING_CONTEXT(DeletedBuffers(this, n, names));
     }

     void GLAPIENTRY fDeleteTextures(GLsizei n, GLuint *names) {
         BEFORE_GL_CALL;
         mSymbols.fDeleteTextures(n, names);
         AFTER_GL_CALL;
         TRACKING_CONTEXT(DeletedTextures(this, n, names));
     }

     void GLAPIENTRY fDeleteFramebuffers(GLsizei n, GLuint *names) {
         BEFORE_GL_CALL;
         if (n == 1 && *names == 0) {
            /* Deleting framebuffer 0 causes hangs on the DROID. See bug 623228 */
         } else {
            mSymbols.fDeleteFramebuffers(n, names);
         }
         AFTER_GL_CALL;
         TRACKING_CONTEXT(DeletedFramebuffers(this, n, names));
     }

     void GLAPIENTRY fDeleteRenderbuffers(GLsizei n, GLuint *names) {
         BEFORE_GL_CALL;
         mSymbols.fDeleteRenderbuffers(n, names);
         AFTER_GL_CALL;
         TRACKING_CONTEXT(DeletedRenderbuffers(this, n, names));
     }
#ifdef DEBUG
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

inline PRBool
DoesVendorStringMatch(const char* aVendorString, const char *aWantedVendor)
{
    if (!aVendorString || !aWantedVendor)
        return PR_FALSE;

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
