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
 *   Jeff Gilbert <jgilbert@mozilla.com>
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
#include "GLLibraryLoader.h"
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

typedef char realGLboolean;

#include "GLContextSymbols.h"

#include "mozilla/mozalloc.h"

namespace mozilla {
  namespace layers {
    class LayerManagerOGL;
    class ColorTextureLayerProgram;
  }

namespace gl {
class GLContext;


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

    enum Flags {
        NoFlags          = 0x0,
        UseNearestFilter = 0x1,
        NeedsYFlip       = 0x2
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
     * Retrieves the region that will require updating, given a
     * region that needs to be updated. This can be used for
     * making decisions about updating before calling BeginUpdate().
     *
     * |aRegion| is an inout param.
     */
    virtual void GetUpdateRegion(nsIntRegion& aForRegion) {
    };
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

    virtual bool NextTile() {
        return false;
    };

    // Function prototype for a tile iteration callback. Returning false will
    // cause iteration to be interrupted (i.e. the corresponding NextTile call
    // will return false).
    typedef bool (* TileIterationCallback)(TextureImage* aImage,
                                           int aTileNumber,
                                           void* aCallbackData);

    // Sets a callback to be called every time NextTile is called.
    virtual void SetIterationCallback(TileIterationCallback aCallback,
                                      void* aCallbackData) {
    };

    virtual nsIntRect GetTileRect() {
        return nsIntRect(nsIntPoint(0,0), mSize);
    };

    virtual GLuint GetTextureID() = 0;

    virtual PRUint32 GetTileCount() {
        return 1;
    };

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
     * Mark this texture as having valid contents. Call this after modifying
     * the texture contents externally.
     */
    virtual void MarkValid() {}

    /**
     * aSurf - the source surface to update from
     * aRegion - the region in this image to update
     * aFrom - offset in the source to update from
     */
    virtual bool DirectUpdate(gfxASurface *aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom = nsIntPoint(0,0)) = 0;

    virtual void BindTexture(GLenum aTextureUnit) = 0;
    virtual void ReleaseTexture() {};

    void BindTextureAndApplyFilter(GLenum aTextureUnit) {
        BindTexture(aTextureUnit);
        ApplyFilter();
    }

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

    protected:
        TextureImage *mTexture;
    };

    class ScopedBindTextureAndApplyFilter
        : public ScopedBindTexture
    {
    public:
        ScopedBindTextureAndApplyFilter(TextureImage *aTexture, GLenum aTextureUnit) :
          ScopedBindTexture(aTexture, aTextureUnit)
        {
            if (mTexture) {
                mTexture->ApplyFilter();
            }
        }
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
    virtual bool InUpdate() const = 0;
    GLenum GetWrapMode() const { return mWrapMode; }

    void SetFilter(gfxPattern::GraphicsFilter aFilter) { mFilter = aFilter; }

    /**
     * Applies this TextureImage's filter, assuming that its texture is
     * the currently bound texture.
     */
    virtual void ApplyFilter() = 0;

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
                 Flags aFlags = NoFlags)
        : mSize(aSize)
        , mWrapMode(aWrapMode)
        , mContentType(aContentType)
        , mFilter(gfxPattern::FILTER_GOOD)
        , mFlags(aFlags)
    {}

    virtual nsIntRect GetSrcTileRect() {
        return nsIntRect(nsIntPoint(0,0), mSize);
    };

    nsIntSize mSize;
    GLenum mWrapMode;
    ContentType mContentType;
    ShaderProgramType mShaderType;
    gfxPattern::GraphicsFilter mFilter;
    Flags mFlags;
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
                      GLContext* aContext,
                      TextureImage::Flags aFlags = TextureImage::NoFlags)
        : TextureImage(aSize, aWrapMode, aContentType, aFlags)
        , mTexture(aTexture)
        , mTextureState(Created)
        , mGLContext(aContext)
        , mUpdateOffset(0, 0)
    {}

    virtual void BindTexture(GLenum aTextureUnit);

    virtual gfxASurface* BeginUpdate(nsIntRegion& aRegion);
    virtual void GetUpdateRegion(nsIntRegion& aForRegion);
    virtual void EndUpdate();
    virtual bool DirectUpdate(gfxASurface* aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom = nsIntPoint(0,0));
    virtual GLuint GetTextureID() { return mTexture; };
    // Returns a surface to draw into
    virtual already_AddRefed<gfxASurface>
      GetSurfaceForUpdate(const gfxIntSize& aSize, ImageFormat aFmt);

    virtual void MarkValid() { mTextureState = Valid; }

    // Call when drawing into the update surface is complete.
    // Returns true if textures should be upload with a relative 
    // offset - See UploadSurfaceToTexture.
    virtual bool FinishedSurfaceUpdate();

    // Call after surface data has been uploaded to a texture.
    virtual void FinishedSurfaceUpload();

    virtual bool InUpdate() const { return !!mUpdateSurface; }

    virtual void Resize(const nsIntSize& aSize);

    virtual void ApplyFilter();
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
        TextureImage::ContentType, TextureImage::Flags aFlags = TextureImage::NoFlags);
    ~TiledTextureImage();
    void DumpDiv();
    virtual gfxASurface* BeginUpdate(nsIntRegion& aRegion);
    virtual void GetUpdateRegion(nsIntRegion& aForRegion);
    virtual void EndUpdate();
    virtual void Resize(const nsIntSize& aSize);
    virtual PRUint32 GetTileCount();
    virtual void BeginTileIteration();
    virtual bool NextTile();
    virtual void SetIterationCallback(TileIterationCallback aCallback,
                                      void* aCallbackData);
    virtual nsIntRect GetTileRect();
    virtual GLuint GetTextureID() {
        return mImages[mCurrentImage]->GetTextureID();
    };
    virtual bool DirectUpdate(gfxASurface* aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom = nsIntPoint(0,0));
    virtual bool InUpdate() const { return mInUpdate; };
    virtual void BindTexture(GLenum);
    virtual void ApplyFilter();

protected:
    virtual nsIntRect GetSrcTileRect();

    unsigned int mCurrentImage;
    TileIterationCallback mIterationCallback;
    void* mIterationCallbackData;
    nsTArray< nsRefPtr<TextureImage> > mImages;
    bool mInUpdate;
    nsIntSize mSize;
    unsigned int mTileSize;
    unsigned int mRows, mColumns;
    GLContext* mGL;
    bool mUseNearestFilter;
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
    int samples;

    int colorBits() const { return red + green + blue; }
};

class GLContext
    : public GLLibraryLoader
{
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GLContext)
public:
    GLContext(const ContextFormat& aFormat,
              bool aIsOffscreen = false,
              GLContext *aSharedContext = nsnull)
      : mFlushGuaranteesResolve(false),
        mUserBoundDrawFBO(0),
        mUserBoundReadFBO(0),
        mInternalBoundDrawFBO(0),
        mInternalBoundReadFBO(0),
#ifdef DEBUG
        mInInternalBindingMode_DrawFBO(true),
        mInInternalBindingMode_ReadFBO(true),
#endif
        mOffscreenFBOsDirty(false),
        mInitialized(false),
        mIsOffscreen(aIsOffscreen),
        mIsGLES2(false),
        mIsGlobalSharedContext(false),
        mHasRobustness(false),
        mContextLost(false),
        mVendor(-1),
        mRenderer(-1),
        mCreationFormat(aFormat),
        mSharedContext(aSharedContext),
        mOffscreenTexture(0),
        mFlipped(false),
        mBlitProgram(0),
        mBlitFramebuffer(0),
        mOffscreenDrawFBO(0),
        mOffscreenReadFBO(0),
        mOffscreenColorRB(0),
        mOffscreenDepthRB(0),
        mOffscreenStencilRB(0),
        mMaxTextureSize(0),
        mMaxCubeMapTextureSize(0),
        mMaxTextureImageSize(0),
        mMaxRenderbufferSize(0),
        mWorkAroundDriverBugs(true)
#ifdef DEBUG
        , mGLError(LOCAL_GL_NO_ERROR)
#endif
    {
        mUserData.Init();
        mOwningThread = NS_GetCurrentThread();
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

    enum ContextFlags {
        ContextFlagsNone = 0x0,
        ContextFlagsGlobal = 0x1
    };

    enum GLContextType {
        ContextTypeUnknown,
        ContextTypeWGL,
        ContextTypeCGL,
        ContextTypeGLX,
        ContextTypeEGL,
        ContextTypeOSMesa
    };

    virtual GLContextType GetContextType() { return ContextTypeUnknown; }

    virtual bool MakeCurrentImpl(bool aForce = false) = 0;

#ifdef DEBUG
    static void StaticInit() {
        PR_NewThreadPrivateIndex(&sCurrentGLContextTLS, NULL);
    }
#endif

    bool MakeCurrent(bool aForce = false) {
#ifdef DEBUG
        PR_SetThreadPrivate(sCurrentGLContextTLS, this);
#endif
        return MakeCurrentImpl(aForce);
    }

    bool IsContextLost() { return mContextLost; }

    virtual bool SetupLookupFunction() = 0;

    virtual void WindowDestroyed() {}

    virtual void ReleaseSurface() {}

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

    bool IsDestroyed() {
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

    bool IsGlobalSharedContext() { return mIsGlobalSharedContext; }
    void SetIsGlobalSharedContext(bool aIsOne) { mIsGlobalSharedContext = aIsOne; }

    /**
     * Returns true if the thread on which this context was created is the currently
     * executing thread.
     */
    bool IsOwningThreadCurrent() { return NS_GetCurrentThread() == mOwningThread; }

    void DispatchToOwningThread(nsIRunnable *event) {
        // Before dispatching, we need to ensure we're not in the middle of
        // shutting down. Dispatching runnables in the middle of shutdown
        // (that is, when the main thread is no longer get-able) can cause them
        // to leak. See Bug 741319, and Bug 744115.
        nsCOMPtr<nsIThread> mainThread;
        if (NS_SUCCEEDED(NS_GetMainThread(getter_AddRefs(mainThread)))) {
            mOwningThread->Dispatch(event, NS_DISPATCH_NORMAL);
        }
    }

    const ContextFormat& CreationFormat() { return mCreationFormat; }
    const ContextFormat& ActualFormat() { return mActualFormat; }

    /**
     * If this GL context has a D3D texture share handle, returns non-null.
     */
    virtual void *GetD3DShareHandle() { return nsnull; }

    /**
     * If this context is double-buffered, returns TRUE.
     */
    virtual bool IsDoubleBuffered() { return false; }

    /**
     * If this context is the GLES2 API, returns TRUE.
     * This means that various GLES2 restrictions might be in effect (modulo
     * extensions).
     */
    bool IsGLES2() const {
        return mIsGLES2;
    }
    
    /**
     * Returns true if either this is the GLES2 API, or had the GL_ARB_ES2_compatibility extension
     */
    bool HasES2Compatibility() {
        return mIsGLES2 || IsExtensionSupported(ARB_ES2_compatibility);
    }

    /**
     * Returns true if the context is using ANGLE. This should only be overridden for an ANGLE
     * implementation.
     */
    virtual bool IsANGLE() {
        return false;
    }

    /**
     * The derived class is expected to provide information on whether or not it
     * supports robustness.
     */
    virtual bool SupportsRobustness() = 0;

    enum {
        VendorIntel,
        VendorNVIDIA,
        VendorATI,
        VendorQualcomm,
        VendorImagination,
        VendorOther
    };

    enum {
        RendererAdreno200,
        RendererAdreno205,
        RendererSGX530,
        RendererSGX540,
        RendererOther
    };

    int Vendor() const {
        return mVendor;
    }

    int Renderer() const {
        return mRenderer;
    }

    bool CanUploadSubTextures();
    bool CanUploadNonPowerOfTwo();
    bool WantsSmallTiles();
    virtual bool HasLockSurface() { return false; }

    /**
     * If this context wraps a double-buffered target, swap the back
     * and front buffers.  It should be assumed that after a swap, the
     * contents of the new back buffer are undefined.
     */
    virtual bool SwapBuffers() { return false; }

    /**
     * Defines a two-dimensional texture image for context target surface
     */
    virtual bool BindTexImage() { return false; }
    /*
     * Releases a color buffer that is being used as a texture
     */
    virtual bool ReleaseTexImage() { return false; }

    /**
     * Applies aFilter to the texture currently bound to GL_TEXTURE_2D.
     */
    void ApplyFilterToBoundTexture(gfxPattern::GraphicsFilter aFilter);


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
    virtual bool BindOffscreenNeedsTexture(GLContext *aOffscreen) {
        return aOffscreen->mOffscreenTexture == 0;
    }

    virtual bool BindTex2DOffscreen(GLContext *aOffscreen) {
        if (aOffscreen->GetContextType() != GetContextType()) {
          return false;
        }

        if (!aOffscreen->mSharedContext ||
            aOffscreen->mSharedContext != mSharedContext)
        {
            return false;
        }

        if (!aOffscreen->mOffscreenTexture) {
            return false;
        }

        fBindTexture(LOCAL_GL_TEXTURE_2D, aOffscreen->mOffscreenTexture);

        return true;
    }

    virtual void UnbindTex2DOffscreen(GLContext *aOffscreen) { }

    bool IsOffscreen() {
        return mIsOffscreen;
    }

private:
    bool mFlushGuaranteesResolve;

public:
    void SetFlushGuaranteesResolve(bool aFlushGuaranteesResolve) {
        mFlushGuaranteesResolve = aFlushGuaranteesResolve;
    }
    
    // Before reads from offscreen texture
    void GuaranteeResolve() {
        if (mFlushGuaranteesResolve) {
            BlitDirtyFBOs();
            fFlush();
        } else {
            fFinish();
        }
    }

    /*
     * Resize the current offscreen buffer.  Returns true on success.
     * If it returns false, the context should be treated as unusable
     * and should be recreated.  After the resize, the viewport is not
     * changed; glViewport should be called as appropriate.
     *
     * Only valid if IsOffscreen() returns true.
     */
    virtual bool ResizeOffscreen(const gfxIntSize& aNewSize) {
        if (mOffscreenDrawFBO || mOffscreenReadFBO)
            return ResizeOffscreenFBOs(aNewSize, mOffscreenReadFBO != 0);
        return false;
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
        // 0 is interpreted as (off)screen, whether for read or draw operations
        return 0;
    }

    GLuint GetOffscreenTexture() {
        return mOffscreenTexture;
    }

    virtual bool SupportsFramebufferMultisample() {
        return IsExtensionSupported(EXT_framebuffer_multisample) || IsExtensionSupported(ANGLE_framebuffer_multisample);
    }

    virtual bool SupportsOffscreenSplit() {
        return IsExtensionSupported(EXT_framebuffer_blit) || IsExtensionSupported(ANGLE_framebuffer_blit);
    }



private:
    GLuint mUserBoundDrawFBO;
    GLuint mUserBoundReadFBO;
    GLuint mInternalBoundDrawFBO;
    GLuint mInternalBoundReadFBO;

public:
    void fBindFramebuffer(GLenum target, GLuint framebuffer) {
        switch (target) {
          case LOCAL_GL_DRAW_FRAMEBUFFER_EXT:
            mUserBoundDrawFBO = framebuffer;

            if (framebuffer == 0) {
                mInternalBoundDrawFBO = mOffscreenDrawFBO;
            } else {
                mInternalBoundDrawFBO = mUserBoundDrawFBO;
            }

            raw_fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT,
                                 mInternalBoundDrawFBO);
            break;

          case LOCAL_GL_READ_FRAMEBUFFER_EXT:
            mUserBoundReadFBO = framebuffer;

            if (framebuffer == 0) {
                mInternalBoundReadFBO = mOffscreenReadFBO;
            } else {
                mInternalBoundReadFBO = mUserBoundReadFBO;
            }

            raw_fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT,
                                 mInternalBoundReadFBO);
            break;

          case LOCAL_GL_FRAMEBUFFER:
            mUserBoundDrawFBO = mUserBoundReadFBO = framebuffer;

            if (framebuffer == 0) {
                mInternalBoundDrawFBO = mOffscreenDrawFBO;
                mInternalBoundReadFBO = mOffscreenReadFBO;
            } else {
                mInternalBoundDrawFBO = mUserBoundDrawFBO;
                mInternalBoundReadFBO = mUserBoundReadFBO;
            }

            if (SupportsOffscreenSplit()) {
                raw_fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT,
                                     mInternalBoundDrawFBO);
                raw_fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT,
                                     mInternalBoundReadFBO);
            } else {
                raw_fBindFramebuffer(LOCAL_GL_FRAMEBUFFER,
                                     mInternalBoundDrawFBO);
            }

            break;

          default:
            raw_fBindFramebuffer(target, framebuffer);
            break;
        }
    }

#ifdef DEBUG
    // See comment near BindInternalDrawFBO()
    bool mInInternalBindingMode_DrawFBO;
    bool mInInternalBindingMode_ReadFBO;
#endif

    GLuint GetUserBoundDrawFBO() {
#ifdef DEBUG
        GLint ret = 0;
        // Don't need a branch here, because:
        // LOCAL_GL_DRAW_FRAMEBUFFER_BINDING_EXT == LOCAL_GL_FRAMEBUFFER_BINDING == 0x8CA6
        // We use raw_ here because this is debug code and we need to see what
        // the driver thinks.
        raw_fGetIntegerv(LOCAL_GL_DRAW_FRAMEBUFFER_BINDING_EXT, &ret);

        bool abort = false;

        if (mInInternalBindingMode_DrawFBO) {
            NS_ERROR("Draw FBO still bound internally!");
            printf_stderr("Current internal draw FBO: %d, user: %d)\n", ret, mUserBoundDrawFBO);
            abort = true;
        }

        if (mInternalBoundDrawFBO != (GLuint)ret) {
            NS_ERROR("Draw FBO binding misprediction!");
            printf_stderr("Bound draw FBO was: %d, Expected: %d\n", ret, mInternalBoundDrawFBO);
            abort = true;
        }

        if (abort)
            NS_ABORT();
#endif

        // We only ever expose the user's bound FBOs
        return mUserBoundDrawFBO;
    }

    GLuint GetUserBoundReadFBO() {
#ifdef DEBUG
        GLint ret = 0;
        // We use raw_ here because this is debug code and we need to see what
        // the driver thinks.
        if (SupportsOffscreenSplit())
            raw_fGetIntegerv(LOCAL_GL_READ_FRAMEBUFFER_BINDING_EXT, &ret);
        else
            raw_fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, &ret);

        bool abort = false;

        if (mInInternalBindingMode_ReadFBO) {
            NS_ERROR("Read FBO still bound internally!");
            printf_stderr("Current internal read FBO: %d, user: %d)\n", ret, mUserBoundReadFBO);
            abort = true;
        }

        if (mInternalBoundReadFBO != (GLuint)ret) {
            NS_ERROR("Read FBO binding misprediction!");
            printf_stderr("Bound read FBO was: %d, Expected: %d\n", ret, mInternalBoundReadFBO);
            abort = true;
        }

        if (abort)
            NS_ABORT();
#endif

        // We only ever expose the user's bound FBOs
        return mUserBoundReadFBO;
    }

    void BindUserDrawFBO(GLuint name) {
        if (SupportsOffscreenSplit())
            fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT, name);
        else
            fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, name);
#ifdef DEBUG
        mInInternalBindingMode_DrawFBO = false;
#endif
    }

    void BindUserReadFBO(GLuint name) {
        if (SupportsOffscreenSplit())
            fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT, name);
        else
            fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, name);
#ifdef DEBUG
        mInInternalBindingMode_ReadFBO = false;
#endif
    }

    // BindInternalDraw/ReadFBO() switch us over into 'internal binding mode'
    //   for the corresponding Draw or Read binding.
    // To exit internal binding mode, use BindUserDraw/ReadFBO().
    // While in internal binding mode for Draw/Read, the corresponding
    //   GetBoundUserDraw/ReadFBO() is undefined, and will trigger ABORT in DEBUG builds.
    void BindInternalDrawFBO(GLuint name) {
#ifdef DEBUG
      mInInternalBindingMode_DrawFBO = true;
#endif
        if (SupportsOffscreenSplit())
            raw_fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER_EXT, name);
        else
            raw_fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, name);

        mInternalBoundDrawFBO = name;
    }

    void BindInternalReadFBO(GLuint name) {
#ifdef DEBUG
      mInInternalBindingMode_ReadFBO = true;
#endif
        if (SupportsOffscreenSplit())
            raw_fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER_EXT, name);
        else
            raw_fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, name);

        mInternalBoundReadFBO = name;
    }

    void BindInternalFBO(GLuint name) {
        BindInternalDrawFBO(name);
        BindInternalReadFBO(name);
    }

    void InitFramebuffers() {
        MakeCurrent();
        BindUserDrawFBO(0);
        BindUserReadFBO(0);
    }

    GLuint SwapUserDrawFBO(GLuint name) {
        GLuint prev = GetUserBoundDrawFBO();
        BindUserDrawFBO(name);
        return prev;
    }

    GLuint SwapUserReadFBO(GLuint name) {
        GLuint prev = GetUserBoundReadFBO();
        BindUserReadFBO(name);
        return prev;
    }

private:
    bool mOffscreenFBOsDirty;

    void GetShaderPrecisionFormatNonES2(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
        switch (precisiontype) {
            case LOCAL_GL_LOW_FLOAT:
            case LOCAL_GL_MEDIUM_FLOAT:
            case LOCAL_GL_HIGH_FLOAT:
                // Assume IEEE 754 precision
                range[0] = 127;
                range[1] = 127;
                *precision = 0;
                break;
            case LOCAL_GL_LOW_INT:
            case LOCAL_GL_MEDIUM_INT:
            case LOCAL_GL_HIGH_INT:
                // Some (most) hardware only supports single-precision floating-point numbers,
                // which can accurately represent integers up to +/-16777216
                range[0] = 24;
                range[1] = 24;
                *precision = 0;
                break;
        }
    }

    // Do whatever setup is necessary to draw to our offscreen FBO, if it's
    // bound.
    void BeforeGLDrawCall() {
        if (mInternalBoundDrawFBO != mOffscreenDrawFBO)
            return;

        if (mOffscreenDrawFBO == mOffscreenReadFBO)
            return;

        mOffscreenFBOsDirty = true;
    }

    // Do whatever tear-down is necessary after drawing to our offscreen FBO,
    // if it's bound.
    void AfterGLDrawCall() {
    }

    // Do whatever setup is necessary to read from our offscreen FBO, if it's
    // bound.
    void BeforeGLReadCall() {
        if (mInternalBoundReadFBO != mOffscreenReadFBO)
            return;

        if (mOffscreenDrawFBO == mOffscreenReadFBO)
            return;

        // If we're not dirty, there's no need to blit
        if (!mOffscreenFBOsDirty)
            return;

        const bool scissor = fIsEnabled(LOCAL_GL_SCISSOR_TEST);
        if (scissor)
            fDisable(LOCAL_GL_SCISSOR_TEST);

        // Store current bindings for restoring later
        GLuint prevDraw = GetUserBoundDrawFBO();
        GLuint prevRead = GetUserBoundReadFBO();

        NS_ABORT_IF_FALSE(SupportsOffscreenSplit(), "Doesn't support offscreen split?");

        // Manually setting internal bindings, entering internal mode
        // Flip read/draw for blitting
        BindInternalDrawFBO(mOffscreenReadFBO);
        BindInternalReadFBO(mOffscreenDrawFBO);

        GLint width = mOffscreenActualSize.width;
        GLint height = mOffscreenActualSize.height;
        raw_fBlitFramebuffer(0, 0, width, height,
                             0, 0, width, height,
                             LOCAL_GL_COLOR_BUFFER_BIT,
                             LOCAL_GL_NEAREST);

        // Reset to emulated user binding, exiting internal mode
        BindUserDrawFBO(prevDraw);
        BindUserReadFBO(prevRead);

        if (scissor)
            fEnable(LOCAL_GL_SCISSOR_TEST);

        mOffscreenFBOsDirty = false;
    }

    // Do whatever tear-down is necessary after reading from our offscreen FBO,
    // if it's bound.
    void AfterGLReadCall() {
    }

public:
    // Draw call hooks:
    void fClear(GLbitfield mask) {
        BeforeGLDrawCall();
        raw_fClear(mask);
        AfterGLDrawCall();
    }

    void fDrawArrays(GLenum mode, GLint first, GLsizei count) {
        BeforeGLDrawCall();
        raw_fDrawArrays(mode, first, count);
        AfterGLDrawCall();
    }

    void fDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
        BeforeGLDrawCall();
        raw_fDrawElements(mode, count, type, indices);
        AfterGLDrawCall();
    }

    // Read call hooks:
    void fReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) {
        BeforeGLReadCall();
        raw_fReadPixels(x, y, width, height, format, type, pixels);
        AfterGLReadCall();
    }

    void fCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
        BeforeGLReadCall();
        raw_fCopyTexImage2D(target, level, internalformat,
                            x, y, width, height, border);
        AfterGLReadCall();
    }

    void fCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
        BeforeGLReadCall();
        raw_fCopyTexSubImage2D(target, level, xoffset, yoffset,
                               x, y, width, height);
        AfterGLReadCall();
    }

    void ForceDirtyFBOs() {
        GLuint draw = SwapUserDrawFBO(0);

        BeforeGLDrawCall();
        // no-op; just pretend we did something
        AfterGLDrawCall();

        BindUserDrawFBO(draw);
    }

    void BlitDirtyFBOs() {
        GLuint read = SwapUserReadFBO(0);

        BeforeGLReadCall();
        // no-op; we just want to make sure the Read FBO is updated if it needs to be
        AfterGLReadCall();

        BindUserReadFBO(read);
    }

    void fFinish() {
        BeforeGLReadCall();
        raw_fFinish();
        AfterGLReadCall();
    }

    // Draw/Read
    void fBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
        BeforeGLDrawCall();
        BeforeGLReadCall();
        raw_fBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
        AfterGLReadCall();
        AfterGLDrawCall();
    }

#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
    virtual gfxASurface* GetOffscreenPixmapSurface()
    {
      return 0;
    };
    
    virtual bool WaitNative() { return false; }
#endif

    virtual bool TextureImageSupportsGetBackingSurface() {
        return false;
    }

    virtual bool RenewSurface() { return false; }

    /**`
     * Return a valid, allocated TextureImage of |aSize| with
     * |aContentType|.  The TextureImage's texture is configured to
     * use |aWrapMode| (usually GL_CLAMP_TO_EDGE or GL_REPEAT) and by
     * default, GL_LINEAR filtering.  Specify
     * |aFlags=UseNearestFilter| for GL_NEAREST filtering. Specify
     * |aFlags=NeedsYFlip| if the image is flipped. Return
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
                       TextureImage::Flags aFlags = TextureImage::NoFlags);

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
                TextureImage::Flags aFlags = TextureImage::NoFlags)
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
                                                       GLenum aTextureFormat,
                                                       bool aYInvert = false);

    already_AddRefed<gfxImageSurface> GetTexImage(GLuint aTexture, bool aYInvert, ShaderProgramType aShader);

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
     * \param aTextureUnit, the texture unit used temporarily to upload the
     *  surface. This testure may be overridden, clients should not rely on
     *  the contents of this texture after this call or even on this
     *  texture unit being active.
     * \return Shader program needed to render this texture.
     */
    ShaderProgramType UploadSurfaceToTexture(gfxASurface *aSurface, 
                                             const nsIntRegion& aDstRegion,
                                             GLuint& aTexture,
                                             bool aOverwrite = false,
                                             const nsIntPoint& aSrcPoint = nsIntPoint(0, 0),
                                             bool aPixelBuffer = false,
                                             GLenum aTextureUnit = LOCAL_GL_TEXTURE0);

    
    void TexImage2D(GLenum target, GLint level, GLint internalformat, 
                    GLsizei width, GLsizei height, GLsizei stride,
                    GLint pixelsize, GLint border, GLenum format, 
                    GLenum type, const GLvoid *pixels);

    void TexSubImage2D(GLenum target, GLint level, 
                       GLint xoffset, GLint yoffset, 
                       GLsizei width, GLsizei height, GLsizei stride,
                       GLint pixelsize, GLenum format, 
                       GLenum type, const GLvoid* pixels);

    /**
     * Uses the Khronos GL_EXT_unpack_subimage extension, working around
     * quirks in the Tegra implementation of this extension.
     */
    void TexSubImage2DWithUnpackSubimageGLES(GLenum target, GLint level,
                                             GLint xoffset, GLint yoffset,
                                             GLsizei width, GLsizei height,
                                             GLsizei stride, GLint pixelsize,
                                             GLenum format, GLenum type,
                                             const GLvoid* pixels);

    void TexSubImage2DWithoutUnpackSubimage(GLenum target, GLint level,
                                            GLint xoffset, GLint yoffset,
                                            GLsizei width, GLsizei height,
                                            GLsizei stride, GLint pixelsize,
                                            GLenum format, GLenum type,
                                            const GLvoid* pixels);

    /** Helper for DecomposeIntoNoRepeatTriangles
     */
    struct RectTriangles {
        RectTriangles() { }

        // Always pass texture coordinates upright. If you want to flip the
        // texture coordinates emitted to the tex_coords array, set flip_y to
        // true.
        void addRect(GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1,
                     GLfloat tx0, GLfloat ty0, GLfloat tx1, GLfloat ty1,
                     bool flip_y = false);

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
     * we don't have to use the REPEAT wrap mode. If aFlipY is true, the
     * texture coordinates will be specified vertically flipped.
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
                                               RectTriangles& aRects,
                                               bool aFlipY = false);

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
        EXT_unpack_subimage,
        OES_standard_derivatives,
        EXT_texture_filter_anisotropic,
        EXT_framebuffer_blit,
        ANGLE_framebuffer_blit,
        EXT_framebuffer_multisample,
        ANGLE_framebuffer_multisample,
        OES_rgb8_rgba8,
        ARB_robustness,
        EXT_robustness,
        ARB_sync,
        Extensions_Max
    };

    bool IsExtensionSupported(GLExtensions aKnownExtension) {
        return mAvailableExtensions[aKnownExtension];
    }

    void MarkExtensionUnsupported(GLExtensions aKnownExtension) {
        mAvailableExtensions[aKnownExtension] = 0;
    }

    // Shared code for GL extensions and GLX extensions.
    static bool ListHasExtension(const GLubyte *extensions,
                                 const char *extension);

    GLint GetMaxTextureSize() { return mMaxTextureSize; }
    GLint GetMaxTextureImageSize() { return mMaxTextureImageSize; }
    void SetFlipped(bool aFlipped) { mFlipped = aFlipped; }

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

    /**
     * Context reset constants.
     * These are used to determine who is guilty when a context reset
     * happens.
     */
    enum ContextResetARB {
        CONTEXT_NO_ERROR = 0,
        CONTEXT_GUILTY_CONTEXT_RESET_ARB = 0x8253,
        CONTEXT_INNOCENT_CONTEXT_RESET_ARB = 0x8254,
        CONTEXT_UNKNOWN_CONTEXT_RESET_ARB = 0x8255
    };

    bool HasRobustness() {
        return mHasRobustness;
    }

protected:
    bool mInitialized;
    bool mIsOffscreen;
    bool mIsGLES2;
    bool mIsGlobalSharedContext;
    bool mHasRobustness;
    bool mContextLost;

    PRInt32 mVendor;
    PRInt32 mRenderer;

public:
    enum {
        DebugEnabled = 1 << 0,
        DebugTrace = 1 << 1,
        DebugAbortOnError = 1 << 2
    };

    static PRUint32 sDebugMode;

    static PRUint32 DebugMode() {
#ifdef DEBUG
        return sDebugMode;
#else
        return 0;
#endif
    }

protected:

    ContextFormat mCreationFormat;
    nsRefPtr<GLContext> mSharedContext;

    // The thread on which this context was created.
    nsCOMPtr<nsIThread> mOwningThread;

    GLContextSymbols mSymbols;

#ifdef DEBUG
    // GLDebugMode will check that we don't send call
    // to a GLContext that isn't current on the current
    // thread.
    // Store the current context when binding to thread local
    // storage to support DebugMode on an arbitrary thread.
    static PRUintn sCurrentGLContextTLS;
#endif

    void UpdateActualFormat();
    ContextFormat mActualFormat;

    gfxIntSize mOffscreenSize;
    gfxIntSize mOffscreenActualSize;
    GLuint mOffscreenTexture;
    bool mFlipped;

    // lazy-initialized things
    GLuint mBlitProgram, mBlitFramebuffer;
    void UseBlitProgram();
    void SetBlitFramebufferForDestTexture(GLuint aTexture);

    // Helper to create/resize an offscreen FBO,
    // for offscreen implementations that use FBOs.
    // Note that it does -not- clear the resized buffers.
    bool ResizeOffscreenFBOs(const ContextFormat& aCF, const gfxIntSize& aSize, const bool aNeedsReadBuffer);
    bool ResizeOffscreenFBOs(const gfxIntSize& aSize, const bool aNeedsReadBuffer) {
        if (!IsOffscreenSizeAllowed(aSize))
            return false;

        ContextFormat format(mCreationFormat);

        if (format.samples) {
            // AA path
            if (ResizeOffscreenFBOs(format, aSize, aNeedsReadBuffer))
                return true;

            NS_WARNING("ResizeOffscreenFBOs failed to resize an AA context! Falling back to no AA...");
            format.samples = 0;
        }

        if (ResizeOffscreenFBOs(format, aSize, aNeedsReadBuffer))
            return true;

        NS_WARNING("ResizeOffscreenFBOs failed to resize non-AA context!");
        return false;
    }

    struct GLFormats {
        GLFormats()
            : texColor(0)
            , texColorType(0)
            , rbColor(0)
            , depthStencil(0)
            , depth(0)
            , stencil(0)
            , samples(0)
        {}

        GLenum texColor;
        GLenum texColorType;
        GLenum rbColor;
        GLenum depthStencil;
        GLenum depth;
        GLenum stencil;
        GLsizei samples;
    };

    GLFormats ChooseGLFormats(ContextFormat& aCF);
    void CreateTextureForOffscreen(const GLFormats& aFormats, const gfxIntSize& aSize,
                                   GLuint& texture);
    void CreateRenderbuffersForOffscreen(const GLContext::GLFormats& aFormats, const gfxIntSize& aSize,
                                         GLuint& colorMSRB, GLuint& depthRB, GLuint& stencilRB);
    bool AssembleOffscreenFBOs(const GLuint colorMSRB,
                               const GLuint depthRB,
                               const GLuint stencilRB,
                               const GLuint texture,
                               GLuint& drawFBO,
                               GLuint& readFBO);

    void DeleteOffscreenFBOs();

    GLuint mOffscreenDrawFBO;
    GLuint mOffscreenReadFBO;
    GLuint mOffscreenColorRB;
    GLuint mOffscreenDepthRB;
    GLuint mOffscreenStencilRB;

    ExtensionBitset<Extensions_Max> mAvailableExtensions;

    // Clear to transparent black, with 0 depth and stencil,
    // while preserving current ClearColor etc. values.
    // Useful for resizing offscreen buffers.
public:
    void ClearSafely();

    bool WorkAroundDriverBugs() const { return mWorkAroundDriverBugs; }

protected:

    nsDataHashtable<nsPtrHashKey<void>, void*> mUserData;

    void SetIsGLES2(bool aIsGLES2) {
        NS_ASSERTION(!mInitialized, "SetIsGLES2 can only be called before initialization!");
        mIsGLES2 = aIsGLES2;
    }

    bool InitWithPrefix(const char *prefix, bool trygl);

    void InitExtensions();

    virtual already_AddRefed<TextureImage>
    CreateBasicTextureImage(GLuint aTexture,
                            const nsIntSize& aSize,
                            GLenum aWrapMode,
                            TextureImage::ContentType aContentType,
                            GLContext* aContext,
                            TextureImage::Flags aFlags = TextureImage::NoFlags)
    {
        nsRefPtr<BasicTextureImage> teximage(
            new BasicTextureImage(aTexture, aSize, aWrapMode, aContentType, aContext, aFlags));
        return teximage.forget();
    }

    bool IsOffscreenSizeAllowed(const gfxIntSize& aSize) const {
        PRInt32 biggerDimension = NS_MAX(aSize.width, aSize.height);
        PRInt32 maxAllowed = NS_MIN(mMaxRenderbufferSize, mMaxTextureSize);
        return biggerDimension <= maxAllowed;
    }

    nsTArray<nsIntRect> mViewportStack;
    nsTArray<nsIntRect> mScissorStack;

    GLint mMaxTextureSize;
    GLint mMaxCubeMapTextureSize;
    GLint mMaxTextureImageSize;
    GLint mMaxRenderbufferSize;
    bool mWorkAroundDriverBugs;

    bool IsTextureSizeSafeToPassToDriver(GLenum target, GLsizei width, GLsizei height) const {
#ifdef XP_MACOSX
        if (mWorkAroundDriverBugs &&
            mVendor == VendorIntel) {
            // see bug 737182 for 2D textures, bug 684822 for cube map textures.
            // some drivers handle incorrectly some large texture sizes that are below the
            // max texture size that they report. So we check ourselves against our own values
            // (mMax[CubeMap]TextureSize).
            GLsizei maxSize = target == LOCAL_GL_TEXTURE_CUBE_MAP ||
                                (target >= LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
                                target <= LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
                              ? mMaxCubeMapTextureSize
                              : mMaxTextureSize;
            return width <= maxSize && height <= maxSize;
        }
#endif
        return true;
    }

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
        if (DebugMode()) {
            GLContext *currentGLContext = NULL;

            currentGLContext = (GLContext*)PR_GetThreadPrivate(sCurrentGLContextTLS);

            if (DebugMode() & DebugTrace)
                printf_stderr("[gl:%p] > %s\n", this, glFunction);
            if (this != currentGLContext) {
                printf_stderr("Fatal: %s called on non-current context %p. "
                              "The current context for this thread is %p.\n",
                               glFunction, this, currentGLContext);
                NS_ABORT();
            }
        }
    }

    void AfterGLCall(const char* glFunction) {
        if (DebugMode()) {
            // calling fFinish() immediately after every GL call makes sure that if this GL command crashes,
            // the stack trace will actually point to it. Otherwise, OpenGL being an asynchronous API, stack traces
            // tend to be meaningless
            mSymbols.fFinish();
            mGLError = mSymbols.fGetError();
            if (DebugMode() & DebugTrace)
                printf_stderr("[gl:%p] < %s [0x%04x]\n", this, glFunction, mGLError);
            if (mGLError != LOCAL_GL_NO_ERROR) {
                printf_stderr("GL ERROR: %s generated GL error %s(0x%04x)\n", 
                              glFunction,
                              GLErrorToString(mGLError),
                              mGLError);
                if (DebugMode() & DebugAbortOnError)
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
        if (DebugMode()) {
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

        // bug 744888
        if (WorkAroundDriverBugs() &&
            !data &&
            Vendor() == VendorNVIDIA)
        {
            char c = 0;
            mSymbols.fBufferSubData(target, size-1, 1, &c);
        }

        AFTER_GL_CALL;
    }

    void fBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data) {
        BEFORE_GL_CALL;
        mSymbols.fBufferSubData(target, offset, size, data);
        AFTER_GL_CALL;
    }

    void raw_fClear(GLbitfield mask) {
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

    void raw_fDrawArrays(GLenum mode, GLint first, GLsizei count) {
        BEFORE_GL_CALL;
        mSymbols.fDrawArrays(mode, first, count);
        AFTER_GL_CALL;
    }

    void raw_fDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
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

    void raw_fFinish() {
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

private:
    void raw_fGetIntegerv(GLenum pname, GLint *params) {
        BEFORE_GL_CALL;
        mSymbols.fGetIntegerv(pname, params);
        AFTER_GL_CALL;
    }

public:
    void fGetIntegerv(GLenum pname, GLint *params) {
        switch (pname)
        {
            // LOCAL_GL_FRAMEBUFFER_BINDING is equal to
            // LOCAL_GL_DRAW_FRAMEBUFFER_BINDING_EXT, so we don't need two
            // cases.
            case LOCAL_GL_FRAMEBUFFER_BINDING:
                *params = GetUserBoundDrawFBO();
                break;

            case LOCAL_GL_READ_FRAMEBUFFER_BINDING_EXT:
                *params = GetUserBoundReadFBO();
                break;

            default:
                raw_fGetIntegerv(pname, params);
                break;
        }
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

    void fGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *img) {
        if (!mSymbols.fGetTexImage) {
          return;
        }
        BEFORE_GL_CALL;
        mSymbols.fGetTexImage(target, level, format, type, img);
        AFTER_GL_CALL;
    };

    void fGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
    {  
        if (!mSymbols.fGetTexLevelParameteriv) {
          *params = 0;
          return;
        }
        BEFORE_GL_CALL;
        mSymbols.fGetTexLevelParameteriv(target, level, pname, params);
        AFTER_GL_CALL;
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

    void raw_fReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) {
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
        if (IsTextureSizeSafeToPassToDriver(target, width, height)) {
          mSymbols.fTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
        } else {
          // pass wrong values to cause the GL to generate GL_INVALID_VALUE.
          // See bug 737182 and the comment in IsTextureSizeSafeToPassToDriver.
          mSymbols.fTexImage2D(target, -1, internalformat, -1, -1, -1, format, type, nsnull);
        }
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

    void raw_fCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
        BEFORE_GL_CALL;
        if (IsTextureSizeSafeToPassToDriver(target, width, height)) {
          mSymbols.fCopyTexImage2D(target, level, internalformat, 
                                   x, FixYValue(y, height),
                                   width, height, border);

        } else {
          // pass wrong values to cause the GL to generate GL_INVALID_VALUE.
          // See bug 737182 and the comment in IsTextureSizeSafeToPassToDriver.
          mSymbols.fCopyTexImage2D(target, -1, internalformat, 
                                   x, FixYValue(y, height),
                                   -1, -1, -1);

        }
        AFTER_GL_CALL;
    }

    void raw_fCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
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

    void fGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
        BEFORE_GL_CALL;
        if (mIsGLES2) {
            mSymbols.fGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
        } else {
            // Fall back to automatic values because almost all desktop hardware supports the OpenGL standard precisions.
            GetShaderPrecisionFormatNonES2(shadertype, precisiontype, range, precision);
        }
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

private:
    void raw_fBindFramebuffer(GLenum target, GLuint framebuffer) {
        BEFORE_GL_CALL;
        mSymbols.fBindFramebuffer(target, framebuffer);
        AFTER_GL_CALL;
    }

public:
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

    void raw_fBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
        BEFORE_GL_CALL;
        mSymbols.fBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
        AFTER_GL_CALL;
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

    void fRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height) {
        BEFORE_GL_CALL;
        mSymbols.fRenderbufferStorageMultisample(target, samples, internalFormat, width, height);
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

     GLenum GLAPIENTRY fGetGraphicsResetStatus() {
         BEFORE_GL_CALL;
         GLenum ret = mHasRobustness ? mSymbols.fGetGraphicsResetStatus() : 0;
         AFTER_GL_CALL;
         return ret;
     }

     GLsync GLAPIENTRY fFenceSync(GLenum condition, GLbitfield flags) {
         BEFORE_GL_CALL;
         GLsync ret = mSymbols.fFenceSync(condition, flags);
         AFTER_GL_CALL;
         return ret;
     }

     realGLboolean GLAPIENTRY fIsSync(GLsync sync) {
         BEFORE_GL_CALL;
         realGLboolean ret = mSymbols.fIsSync(sync);
         AFTER_GL_CALL;
         return ret;
     }

     void GLAPIENTRY fDeleteSync(GLsync sync) {
         BEFORE_GL_CALL;
         mSymbols.fDeleteSync(sync);
         AFTER_GL_CALL;
     }

     GLenum GLAPIENTRY fClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
         BEFORE_GL_CALL;
         GLenum ret = mSymbols.fClientWaitSync(sync, flags, timeout);
         AFTER_GL_CALL;
         return ret;
     }

     void GLAPIENTRY fWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
         BEFORE_GL_CALL;
         mSymbols.fWaitSync(sync, flags, timeout);
         AFTER_GL_CALL;
     }

     void GLAPIENTRY fGetInteger64v(GLenum pname, GLint64 *params) {
         BEFORE_GL_CALL;
         mSymbols.fGetInteger64v(pname, params);
         AFTER_GL_CALL;
     }

     void GLAPIENTRY fGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values) {
         BEFORE_GL_CALL;
         mSymbols.fGetSynciv(sync, pname, bufSize, length, values);
         AFTER_GL_CALL;
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
            : origin(nsnull), name(0), originDeleted(false)
        { }

        NamedResource(GLContext *aOrigin, GLuint aName)
            : origin(aOrigin), name(aName), originDeleted(false)
        { }

        GLContext *origin;
        GLuint name;
        bool originDeleted;

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

inline bool
DoesStringMatch(const char* aString, const char *aWantedString)
{
    if (!aString || !aWantedString)
        return false;

    const char *occurrence = strstr(aString, aWantedString);

    // aWanted not found
    if (!occurrence)
        return false;

    // aWantedString preceded by alpha character
    if (occurrence != aString && isalpha(*(occurrence-1)))
        return false;

    // aWantedVendor followed by alpha character
    const char *afterOccurrence = occurrence + strlen(aWantedString);
    if (isalpha(*afterOccurrence))
        return false;

    return true;
}

} /* namespace gl */
} /* namespace mozilla */

#endif /* GLCONTEXT_H_ */
