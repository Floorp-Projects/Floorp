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
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#ifndef WEBGLCONTEXT_H_
#define WEBGLCONTEXT_H_

#include <stdarg.h>
#include <vector>

#include "nsTArray.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"

#include "nsIDocShell.h"

#include "nsIDOMWebGLRenderingContext.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsHTMLCanvasElement.h"
#include "nsWeakReference.h"
#include "nsIDOMHTMLElement.h"
#include "nsIJSNativeInitializer.h"
#include "nsIMemoryReporter.h"

#include "GLContextProvider.h"
#include "Layers.h"

#include "CheckedInt.h"

class nsIDocShell;
class nsIPropertyBag;

namespace mozilla {

class WebGLTexture;
class WebGLBuffer;
class WebGLProgram;
class WebGLShader;
class WebGLFramebuffer;
class WebGLRenderbuffer;
class WebGLUniformLocation;

class WebGLZeroingObject;
class WebGLContextBoundObject;

enum FakeBlackStatus { DoNotNeedFakeBlack, DoNeedFakeBlack, DontKnowIfNeedFakeBlack };

struct VertexAttrib0Status {
    enum { Default, EmulatedUninitializedArray, EmulatedInitializedArray };
};

struct BackbufferClearingStatus {
    enum { NotClearedSinceLastPresented, ClearedToDefaultValues, HasBeenDrawnTo };
};

struct WebGLTexelFormat {
    enum { Generic, Auto, RGBA8, RGB8, RGBX8, BGRA8, BGR8, BGRX8, RGBA5551, RGBA4444, RGB565, R8, RA8, A8,
           RGBA32F, RGB32F, A32F, R32F, RA32F };
};

struct WebGLTexelPremultiplicationOp {
    enum { Generic, None, Premultiply, Unmultiply };
};

int GetWebGLTexelFormat(GLenum format, GLenum type);

inline PRBool is_pot_assuming_nonnegative(WebGLsizei x)
{
    return (x & (x-1)) == 0;
}

class WebGLObjectBaseRefPtr
{
protected:
    friend class WebGLZeroingObject;

    WebGLObjectBaseRefPtr()
        : mRawPtr(0)
    {
    }

    WebGLObjectBaseRefPtr(nsISupports *rawPtr)
        : mRawPtr(rawPtr)
    {
    }

    void Zero() {
        if (mRawPtr) {
            // Note: RemoveRefOwner isn't called here, because
            // the entire owner array will be cleared.
            mRawPtr->Release();
            mRawPtr = 0;
        }
    }

protected:
    nsISupports *mRawPtr;
};

template <class T>
class WebGLObjectRefPtr
    : public WebGLObjectBaseRefPtr
{
public:
    typedef T element_type;

    WebGLObjectRefPtr()
    { }

    WebGLObjectRefPtr(const WebGLObjectRefPtr<T>& aSmartPtr)
        : WebGLObjectBaseRefPtr(aSmartPtr.mRawPtr)
    {
        if (mRawPtr) {
            RawPtr()->AddRef();
            RawPtr()->AddRefOwner(this);
        }
    }

    WebGLObjectRefPtr(T *aRawPtr)
        : WebGLObjectBaseRefPtr(aRawPtr)
    {
        if (mRawPtr) {
            RawPtr()->AddRef();
            RawPtr()->AddRefOwner(this);
        }
    }

    WebGLObjectRefPtr(const already_AddRefed<T>& aSmartPtr)
        : WebGLObjectBaseRefPtr(aSmartPtr.mRawPtr)
          // construct from |dont_AddRef(expr)|
    {
        if (mRawPtr) {
            RawPtr()->AddRef();
            RawPtr()->AddRefOwner(this);
        }
    }

    ~WebGLObjectRefPtr() {
        if (mRawPtr) {
            RawPtr()->RemoveRefOwner(this);
            RawPtr()->Release();
        }
    }

    WebGLObjectRefPtr<T>&
    operator=(const WebGLObjectRefPtr<T>& rhs)
    {
        assign_with_AddRef(static_cast<T*>(rhs.mRawPtr));
        return *this;
    }

    WebGLObjectRefPtr<T>&
    operator=(T* rhs)
    {
        assign_with_AddRef(rhs);
        return *this;
    }

    WebGLObjectRefPtr<T>&
    operator=(const already_AddRefed<T>& rhs)
    {
        assign_assuming_AddRef(static_cast<T*>(rhs.mRawPtr));
        return *this;
    }

    T* get() const {
        return const_cast<T*>(static_cast<T*>(mRawPtr));
    }

    operator T*() const {
        return get();
    }

    T* operator->() const {
        NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL WebGLObjectRefPtr with operator->()!");
        return get();
    }

    T& operator*() const {
        NS_PRECONDITION(mRawPtr != 0, "You can't dereference a NULL WebGLObjectRefPtr with operator*()!");
        return *get();
    }

private:
    T* RawPtr() { return static_cast<T*>(mRawPtr); }

    void assign_with_AddRef(T* rawPtr) {
        if (rawPtr) {
            rawPtr->AddRef();
            rawPtr->AddRefOwner(this);
        }

        assign_assuming_AddRef(rawPtr);
    }

    void assign_assuming_AddRef(T* newPtr) {
        T* oldPtr = RawPtr();
        mRawPtr = newPtr;
        if (oldPtr) {
            oldPtr->RemoveRefOwner(this);
            oldPtr->Release();
        }
    }
};

class WebGLBuffer;

struct WebGLVertexAttribData {
    // note that these initial values are what GL initializes vertex attribs to
    WebGLVertexAttribData()
        : buf(0), stride(0), size(4), byteOffset(0),
          type(LOCAL_GL_FLOAT), enabled(PR_FALSE), normalized(PR_FALSE)
    { }

    WebGLObjectRefPtr<WebGLBuffer> buf;
    WebGLuint stride;
    WebGLuint size;
    GLuint byteOffset;
    GLenum type;
    PRBool enabled;
    PRBool normalized;

    GLuint componentSize() const {
        switch(type) {
            case LOCAL_GL_BYTE:
                return sizeof(GLbyte);
                break;
            case LOCAL_GL_UNSIGNED_BYTE:
                return sizeof(GLubyte);
                break;
            case LOCAL_GL_SHORT:
                return sizeof(GLshort);
                break;
            case LOCAL_GL_UNSIGNED_SHORT:
                return sizeof(GLushort);
                break;
            // XXX case LOCAL_GL_FIXED:
            case LOCAL_GL_FLOAT:
                return sizeof(GLfloat);
                break;
            default:
                NS_ERROR("Should never get here!");
                return 0;
        }
    }

    GLuint actualStride() const {
        if (stride) return stride;
        return size * componentSize();
    }
};

struct WebGLContextOptions {
    // these are defaults
    WebGLContextOptions()
        : alpha(true), depth(true), stencil(false),
          premultipliedAlpha(true), antialias(false),
          preserveDrawingBuffer(false)
    { }

    bool operator==(const WebGLContextOptions& other) const {
        return
            alpha == other.alpha &&
            depth == other.depth &&
            stencil == other.stencil &&
            premultipliedAlpha == other.premultipliedAlpha &&
            antialias == other.antialias &&
            preserveDrawingBuffer == other.preserveDrawingBuffer;
    }

    bool operator!=(const WebGLContextOptions& other) const {
        return !operator==(other);
    }

    bool alpha;
    bool depth;
    bool stencil;
    bool premultipliedAlpha;
    bool antialias;
    bool preserveDrawingBuffer;
};

class WebGLContext :
    public nsIDOMWebGLRenderingContext,
    public nsICanvasRenderingContextInternal,
    public nsSupportsWeakReference
{
    friend class WebGLMemoryReporter;

public:
    WebGLContext();
    virtual ~WebGLContext();

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(WebGLContext, nsIDOMWebGLRenderingContext)

    NS_DECL_NSIDOMWEBGLRENDERINGCONTEXT

    // nsICanvasRenderingContextInternal
    NS_IMETHOD SetCanvasElement(nsHTMLCanvasElement* aParentCanvas);
    NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height);
    NS_IMETHOD InitializeWithSurface(nsIDocShell *docShell, gfxASurface *surface, PRInt32 width, PRInt32 height)
        { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Reset()
        { /* (InitializeWithSurface) */ return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Render(gfxContext *ctx, gfxPattern::GraphicsFilter f);
    NS_IMETHOD GetInputStream(const char* aMimeType,
                              const PRUnichar* aEncoderOptions,
                              nsIInputStream **aStream);
    NS_IMETHOD GetThebesSurface(gfxASurface **surface);
    mozilla::TemporaryRef<mozilla::gfx::SourceSurface> GetSurfaceSnapshot()
        { return nsnull; }

    NS_IMETHOD SetIsOpaque(PRBool b) { return NS_OK; };
    NS_IMETHOD SetContextOptions(nsIPropertyBag *aOptions);

    NS_IMETHOD SetIsIPC(PRBool b) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Redraw(const gfxRect&) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Swap(mozilla::ipc::Shmem& aBack,
                    PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h)
                    { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Swap(PRUint32 nativeID,
                    PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h)
                    { return NS_ERROR_NOT_IMPLEMENTED; }

    nsresult SynthesizeGLError(WebGLenum err);
    nsresult SynthesizeGLError(WebGLenum err, const char *fmt, ...);

    nsresult ErrorInvalidEnum(const char *fmt = 0, ...);
    nsresult ErrorInvalidOperation(const char *fmt = 0, ...);
    nsresult ErrorInvalidValue(const char *fmt = 0, ...);
    nsresult ErrorInvalidEnumInfo(const char *info, PRUint32 enumvalue) {
        return ErrorInvalidEnum("%s: invalid enum value 0x%x", info, enumvalue);
    }
    nsresult ErrorOutOfMemory(const char *fmt = 0, ...);
    
    const char *ErrorName(GLenum error);

    WebGLTexture *activeBoundTextureForTarget(WebGLenum target) {
        return target == LOCAL_GL_TEXTURE_2D ? mBound2DTextures[mActiveTexture]
                                             : mBoundCubeMapTextures[mActiveTexture];
    }

    already_AddRefed<CanvasLayer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                                 CanvasLayer *aOldLayer,
                                                 LayerManager *aManager);
    void MarkContextClean() { mInvalidated = PR_FALSE; }

    // a number that increments every time we have an event that causes
    // all context resources to be lost.
    PRUint32 Generation() { return mGeneration.value(); }

    // this is similar to GLContext::ClearSafely, but is more comprehensive
    // (takes care of scissor, stencil write mask, dithering, viewport...)
    // WebGL has more complex needs than GLContext as content controls GL state.
    void ForceClearFramebufferWithDefaultValues(PRUint32 mask, const nsIntRect& viewportRect);

    // if the preserveDrawingBuffer context option is false, we need to clear the back buffer
    // after it's been presented to the compositor. This function does that if needed.
    // See section 2.2 in the WebGL spec.
    void EnsureBackbufferClearedAsNeeded();

    // checks for GL errors, clears any pending GL error, stores the current GL error in currentGLError,
    // and copies it into mWebGLError if it doesn't already have an error set
    void UpdateWebGLErrorAndClearGLError(GLenum *currentGLError) {
        // get and clear GL error in ALL cases
        *currentGLError = gl->GetAndClearError();
        // only store in mWebGLError if is hasn't already recorded an error
        if (!mWebGLError)
            mWebGLError = *currentGLError;
    }
    
    // checks for GL errors, clears any pending GL error,
    // and stores the current GL error into mWebGLError if it doesn't already have an error set
    void UpdateWebGLErrorAndClearGLError() {
        GLenum currentGLError;
        UpdateWebGLErrorAndClearGLError(&currentGLError);
    }

protected:
    void SetDontKnowIfNeedFakeBlack() {
        mFakeBlackStatus = DontKnowIfNeedFakeBlack;
    }

    PRBool NeedFakeBlack();
    void BindFakeBlackTextures();
    void UnbindFakeBlackTextures();

    int WhatDoesVertexAttrib0Need();
    void DoFakeVertexAttrib0(WebGLuint vertexCount);
    void UndoFakeVertexAttrib0();
    void InvalidateFakeVertexAttrib0();

    static CheckedUint32 GetImageSize(WebGLsizei height, 
                                      WebGLsizei width, 
                                      PRUint32 pixelSize,
                                      PRUint32 alignment);

    // Returns x rounded to the next highest multiple of y.
    static CheckedUint32 RoundedToNextMultipleOf(CheckedUint32 x, CheckedUint32 y) {
        return ((x + y - 1) / y) * y;
    }

    nsCOMPtr<nsIDOMHTMLCanvasElement> mCanvasElement;
    nsHTMLCanvasElement *HTMLCanvasElement() {
        return static_cast<nsHTMLCanvasElement*>(mCanvasElement.get());
    }

    nsRefPtr<gl::GLContext> gl;

    PRInt32 mWidth, mHeight;
    CheckedUint32 mGeneration;

    WebGLContextOptions mOptions;

    PRPackedBool mInvalidated;
    PRPackedBool mResetLayer;
    PRPackedBool mVerbose;
    PRPackedBool mOptionsFrozen;

    WebGLuint mActiveTexture;
    WebGLenum mWebGLError;

    // whether shader validation is supported
    PRBool mShaderValidation;

    // some GL constants
    PRInt32 mGLMaxVertexAttribs;
    PRInt32 mGLMaxTextureUnits;
    PRInt32 mGLMaxTextureSize;
    PRInt32 mGLMaxCubeMapTextureSize;
    PRInt32 mGLMaxTextureImageUnits;
    PRInt32 mGLMaxVertexTextureImageUnits;
    PRInt32 mGLMaxVaryingVectors;
    PRInt32 mGLMaxFragmentUniformVectors;
    PRInt32 mGLMaxVertexUniformVectors;

    // extensions
    enum WebGLExtensionID {
        WebGL_OES_texture_float,
        WebGLExtensionID_Max
    };
    nsCOMPtr<nsIWebGLExtension> mEnabledExtensions[WebGLExtensionID_Max];
    PRBool IsExtensionEnabled(WebGLExtensionID ext) const {
        NS_ABORT_IF_FALSE(ext >= 0 && ext < WebGLExtensionID_Max, "bogus index!");
        return mEnabledExtensions[ext] != nsnull;
    }
    bool IsExtensionSupported(WebGLExtensionID ei);

    PRBool InitAndValidateGL();
    PRBool ValidateBuffers(PRInt32* maxAllowedCount, const char *info);
    PRBool ValidateCapabilityEnum(WebGLenum cap, const char *info);
    PRBool ValidateBlendEquationEnum(WebGLenum cap, const char *info);
    PRBool ValidateBlendFuncDstEnum(WebGLenum mode, const char *info);
    PRBool ValidateBlendFuncSrcEnum(WebGLenum mode, const char *info);
    PRBool ValidateBlendFuncEnumsCompatibility(WebGLenum sfactor, WebGLenum dfactor, const char *info);
    PRBool ValidateTextureTargetEnum(WebGLenum target, const char *info);
    PRBool ValidateComparisonEnum(WebGLenum target, const char *info);
    PRBool ValidateStencilOpEnum(WebGLenum action, const char *info);
    PRBool ValidateFaceEnum(WebGLenum face, const char *info);
    PRBool ValidateBufferUsageEnum(WebGLenum target, const char *info);
    PRBool ValidateTexFormatAndType(WebGLenum format, WebGLenum type, int jsArrayType,
                                      PRUint32 *texelSize, const char *info);
    PRBool ValidateDrawModeEnum(WebGLenum mode, const char *info);
    PRBool ValidateAttribIndex(WebGLuint index, const char *info);
    PRBool ValidateStencilParamsForDrawCall();
    
    bool  ValidateGLSLIdentifier(const nsAString& name, const char *info);

    static PRUint32 GetTexelSize(WebGLenum format, WebGLenum type);

    void Invalidate();
    void DestroyResourcesAndContext();

    void MakeContextCurrent() { gl->MakeCurrent(); }

    // helpers
    nsresult TexImage2D_base(WebGLenum target, WebGLint level, WebGLenum internalformat,
                             WebGLsizei width, WebGLsizei height, WebGLsizei srcStrideOrZero, WebGLint border,
                             WebGLenum format, WebGLenum type,
                             void *data, PRUint32 byteLength,
                             int jsArrayType,
                             int srcFormat, PRBool srcPremultiplied);
    nsresult TexSubImage2D_base(WebGLenum target, WebGLint level,
                                WebGLint xoffset, WebGLint yoffset,
                                WebGLsizei width, WebGLsizei height, WebGLsizei srcStrideOrZero,
                                WebGLenum format, WebGLenum type,
                                void *pixels, PRUint32 byteLength,
                                int jsArrayType,
                                int srcFormat, PRBool srcPremultiplied);
    nsresult ReadPixels_base(WebGLint x, WebGLint y, WebGLsizei width, WebGLsizei height,
                             WebGLenum format, WebGLenum type, void *data, PRUint32 byteLength);
    nsresult TexParameter_base(WebGLenum target, WebGLenum pname,
                               WebGLint *intParamPtr, WebGLfloat *floatParamPtr);

    void ConvertImage(size_t width, size_t height, size_t srcStride, size_t dstStride,
                      const PRUint8*src, PRUint8 *dst,
                      int srcFormat, PRBool srcPremultiplied,
                      int dstFormat, PRBool dstPremultiplied,
                      size_t dstTexelSize);

    nsresult DOMElementToImageSurface(nsIDOMElement *imageOrCanvas,
                                      gfxImageSurface **imageOut,
                                      int *format);

    nsresult CopyTexSubImage2D_base(WebGLenum target,
                                    WebGLint level,
                                    WebGLenum internalformat,
                                    WebGLint xoffset,
                                    WebGLint yoffset,
                                    WebGLint x,
                                    WebGLint y,
                                    WebGLsizei width,
                                    WebGLsizei height,
                                    bool sub
                                  );

    // Conversion from public nsI* interfaces to concrete objects
    template<class ConcreteObjectType, class BaseInterfaceType>
    PRBool GetConcreteObject(const char *info,
                             BaseInterfaceType *aInterface,
                             ConcreteObjectType **aConcreteObject,
                             PRBool *isNull = 0,
                             PRBool *isDeleted = 0,
                             PRBool generateErrors = PR_TRUE);

    template<class ConcreteObjectType, class BaseInterfaceType>
    PRBool GetConcreteObjectAndGLName(const char *info,
                                      BaseInterfaceType *aInterface,
                                      ConcreteObjectType **aConcreteObject,
                                      WebGLuint *aGLObjectName,
                                      PRBool *isNull = 0,
                                      PRBool *isDeleted = 0);

    template<class ConcreteObjectType, class BaseInterfaceType>
    PRBool GetGLName(const char *info,
                     BaseInterfaceType *aInterface,
                     WebGLuint *aGLObjectName,
                     PRBool *isNull = 0,
                     PRBool *isDeleted = 0);

    template<class ConcreteObjectType, class BaseInterfaceType>
    PRBool CanGetConcreteObject(const char *info,
                                BaseInterfaceType *aInterface,
                                PRBool *isNull = 0,
                                PRBool *isDeleted = 0);

    PRInt32 MaxTextureSizeForTarget(WebGLenum target) const {
        return target == LOCAL_GL_TEXTURE_2D ? mGLMaxTextureSize : mGLMaxCubeMapTextureSize;
    }
    
    /** like glBufferData but if the call may change the buffer size, checks any GL error generated
     * by this glBufferData call and returns it */
    GLenum CheckedBufferData(GLenum target,
                             GLsizeiptr size,
                             const GLvoid *data,
                             GLenum usage);
    /** like glTexImage2D but if the call may change the texture size, checks any GL error generated
     * by this glTexImage2D call and returns it */
    GLenum CheckedTexImage2D(GLenum target,
                             GLint level,
                             GLenum internalFormat,
                             GLsizei width,
                             GLsizei height,
                             GLint border,
                             GLenum format,
                             GLenum type,
                             const GLvoid *data);

    // the buffers bound to the current program's attribs
    nsTArray<WebGLVertexAttribData> mAttribBuffers;

    // the textures bound to any sampler uniforms
    nsTArray<WebGLObjectRefPtr<WebGLTexture> > mUniformTextures;

    // textures bound to 
    nsTArray<WebGLObjectRefPtr<WebGLTexture> > mBound2DTextures;
    nsTArray<WebGLObjectRefPtr<WebGLTexture> > mBoundCubeMapTextures;

    WebGLObjectRefPtr<WebGLBuffer> mBoundArrayBuffer;
    WebGLObjectRefPtr<WebGLBuffer> mBoundElementArrayBuffer;
    // note nsRefPtr -- this stays alive even after being deleted,
    // and is only explicitly removed from the current state via
    // a call to UseProgram.
    nsRefPtr<WebGLProgram> mCurrentProgram;

    PRUint32 mMaxFramebufferColorAttachments;

    nsRefPtr<WebGLFramebuffer> mBoundFramebuffer;
    nsRefPtr<WebGLRenderbuffer> mBoundRenderbuffer;

    // lookup tables for GL name -> object wrapper
    nsRefPtrHashtable<nsUint32HashKey, WebGLTexture> mMapTextures;
    nsRefPtrHashtable<nsUint32HashKey, WebGLBuffer> mMapBuffers;
    nsRefPtrHashtable<nsUint32HashKey, WebGLProgram> mMapPrograms;
    nsRefPtrHashtable<nsUint32HashKey, WebGLShader> mMapShaders;
    nsRefPtrHashtable<nsUint32HashKey, WebGLFramebuffer> mMapFramebuffers;
    nsRefPtrHashtable<nsUint32HashKey, WebGLRenderbuffer> mMapRenderbuffers;

    // PixelStore parameters
    PRUint32 mPixelStorePackAlignment, mPixelStoreUnpackAlignment, mPixelStoreColorspaceConversion;
    PRBool mPixelStoreFlipY, mPixelStorePremultiplyAlpha;

    FakeBlackStatus mFakeBlackStatus;

    WebGLuint mBlackTexture2D, mBlackTextureCubeMap;
    PRBool mBlackTexturesAreInitialized;

    WebGLfloat mVertexAttrib0Vector[4];
    WebGLfloat mFakeVertexAttrib0BufferObjectVector[4];
    size_t mFakeVertexAttrib0BufferObjectSize;
    GLuint mFakeVertexAttrib0BufferObject;
    int mFakeVertexAttrib0BufferStatus;

    WebGLint mStencilRefFront, mStencilRefBack;
    WebGLuint mStencilValueMaskFront, mStencilValueMaskBack,
              mStencilWriteMaskFront, mStencilWriteMaskBack;
    realGLboolean mColorWriteMask[4];
    realGLboolean mDepthWriteMask;
    realGLboolean mScissorTestEnabled;
    realGLboolean mDitherEnabled;
    WebGLfloat mColorClearValue[4];
    WebGLint mStencilClearValue;
    WebGLfloat mDepthClearValue;

    int mBackbufferClearingStatus;

public:
    // console logging helpers
    static void LogMessage(const char *fmt, ...);
    static void LogMessage(const char *fmt, va_list ap);
    void LogMessageIfVerbose(const char *fmt, ...);
    void LogMessageIfVerbose(const char *fmt, va_list ap);

    friend class WebGLTexture;
    friend class WebGLFramebuffer;
};

// this class is a mixin for the named type wrappers, and is used
// by WebGLObjectRefPtr to tell the object who holds references, so that
// we can zero them out appropriately when the object is deleted, because
// it will be unbound in the GL.
class WebGLZeroingObject
{
public:
    WebGLZeroingObject()
    { }

    void AddRefOwner(WebGLObjectBaseRefPtr *owner) {
        mRefOwners.AppendElement(owner);
    }

    void RemoveRefOwner(WebGLObjectBaseRefPtr *owner) {
        mRefOwners.RemoveElement(owner);
    }

    void ZeroOwners() {
        WebGLObjectBaseRefPtr **owners = mRefOwners.Elements();
        
        for (PRUint32 i = 0; i < mRefOwners.Length(); i++) {
            owners[i]->Zero();
        }

        mRefOwners.Clear();
    }

protected:
    nsTArray<WebGLObjectBaseRefPtr *> mRefOwners;
};

// this class is a mixin for GL objects that have dimensions
// that we need to track.
class WebGLRectangleObject
{
protected:
    WebGLRectangleObject()
        : mWidth(0), mHeight(0) { }

public:
    WebGLsizei width() const { return mWidth; }
    void width(WebGLsizei value) { mWidth = value; }

    WebGLsizei height() const { return mHeight; }
    void height(WebGLsizei value) { mHeight = value; }

    void setDimensions(WebGLsizei width, WebGLsizei height) {
        mWidth = width;
        mHeight = height;
    }

    void setDimensions(WebGLRectangleObject *rect) {
        if (rect) {
            mWidth = rect->width();
            mHeight = rect->height();
        } else {
            mWidth = 0;
            mHeight = 0;
        }
    }

    bool HasSameDimensionsAs(const WebGLRectangleObject& other) const {
        return width() == other.width() && height() == other.height(); 
    }

protected:
    WebGLsizei mWidth;
    WebGLsizei mHeight;
};

// This class is a mixin for objects that are tied to a specific
// context (which is to say, all of them).  They provide initialization
// as well as comparison with the current context.
class WebGLContextBoundObject
{
public:
    WebGLContextBoundObject(WebGLContext *context) {
        mContext = context;
        mContextGeneration = context->Generation();
    }

    PRBool IsCompatibleWithContext(WebGLContext *other) {
        return mContext == other &&
            mContextGeneration == other->Generation();
    }

protected:
    WebGLContext *mContext;
    PRUint32 mContextGeneration;
};

#define WEBGLBUFFER_PRIVATE_IID \
    {0xd69f22e9, 0x6f98, 0x48bd, {0xb6, 0x94, 0x34, 0x17, 0xed, 0x06, 0x11, 0xab}}
class WebGLBuffer :
    public nsIWebGLBuffer,
    public WebGLZeroingObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLBUFFER_PRIVATE_IID)

    WebGLBuffer(WebGLContext *context, WebGLuint name) :
        WebGLContextBoundObject(context),
        mName(name), mDeleted(PR_FALSE), mHasEverBeenBound(PR_FALSE),
        mByteLength(0), mTarget(LOCAL_GL_NONE), mData(nsnull)
    {}

    ~WebGLBuffer() {
        Delete();
    }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();

        free(mData);
        mData = nsnull;

        mDeleted = PR_TRUE;
        mByteLength = 0;
    }

    PRBool Deleted() const { return mDeleted; }
    PRBool HasEverBeenBound() { return mHasEverBeenBound; }
    void SetHasEverBeenBound(PRBool x) { mHasEverBeenBound = x; }
    GLuint GLName() const { return mName; }
    GLuint ByteLength() const { return mByteLength; }
    GLenum Target() const { return mTarget; }
    const void *Data() const { return mData; }

    void SetByteLength(GLuint byteLength) { mByteLength = byteLength; }
    void SetTarget(GLenum target) { mTarget = target; }

    // element array buffers are the only buffers for which we need to keep a copy of the data.
    // this method assumes that the byte length has previously been set by calling SetByteLength.
    PRBool CopyDataIfElementArray(const void* data) {
        if (mTarget == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
            mData = realloc(mData, mByteLength);
            if (!mData) {
                mByteLength = 0;
                return PR_FALSE;
            }
            memcpy(mData, data, mByteLength);
        }
        return PR_TRUE;
    }

    // same comments as for CopyElementArrayData
    PRBool ZeroDataIfElementArray() {
        if (mTarget == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
            mData = realloc(mData, mByteLength);
            if (!mData) {
                mByteLength = 0;
                return PR_FALSE;
            }
            memset(mData, 0, mByteLength);
        }
        return PR_TRUE;
    }

    // same comments as for CopyElementArrayData
    void CopySubDataIfElementArray(GLuint byteOffset, GLuint byteLength, const void* data) {
        if (mTarget == LOCAL_GL_ELEMENT_ARRAY_BUFFER && mByteLength) {
            memcpy((void*) (size_t(mData)+byteOffset), data, byteLength);
        }
    }

    // this method too is only for element array buffers. It returns the maximum value in the part of
    // the buffer starting at given offset, consisting of given count of elements. The type T is the type
    // to interprete the array elements as, must be GLushort or GLubyte.
    template<typename T>
    PRInt32 FindMaxElementInSubArray(GLuint count, GLuint byteOffset)
    {
        const T* start = reinterpret_cast<T*>(reinterpret_cast<size_t>(mData) + byteOffset);
        const T* stop = start + count;
        T result = 0;
        for(const T* ptr = start; ptr != stop; ++ptr) {
            if (*ptr > result) result = *ptr;
        }
        return result;
    }

    void InvalidateCachedMaxElements() {
      mHasCachedMaxUbyteElement = PR_FALSE;
      mHasCachedMaxUshortElement = PR_FALSE;
    }

    PRInt32 FindMaxUbyteElement() {
      if (mHasCachedMaxUbyteElement) {
        return mCachedMaxUbyteElement;
      } else {
        mHasCachedMaxUbyteElement = PR_TRUE;
        mCachedMaxUbyteElement = FindMaxElementInSubArray<GLubyte>(mByteLength, 0);
        return mCachedMaxUbyteElement;
      }
    }

    PRInt32 FindMaxUshortElement() {
      if (mHasCachedMaxUshortElement) {
        return mCachedMaxUshortElement;
      } else {
        mHasCachedMaxUshortElement = PR_TRUE;
        mCachedMaxUshortElement = FindMaxElementInSubArray<GLshort>(mByteLength>>1, 0);
        return mCachedMaxUshortElement;
      }
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLBUFFER
protected:
    WebGLuint mName;
    PRBool mDeleted;
    PRBool mHasEverBeenBound;
    GLuint mByteLength;
    GLenum mTarget;

    PRUint8 mCachedMaxUbyteElement;
    PRBool mHasCachedMaxUbyteElement;
    PRUint16 mCachedMaxUshortElement;
    PRBool mHasCachedMaxUshortElement;

    void* mData; // in the case of an Element Array Buffer, we keep a copy.
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLBuffer, WEBGLBUFFER_PRIVATE_IID)

#define WEBGLTEXTURE_PRIVATE_IID \
    {0x4c19f189, 0x1f86, 0x4e61, {0x96, 0x21, 0x0a, 0x11, 0xda, 0x28, 0x10, 0xdd}}
class WebGLTexture :
    public nsIWebGLTexture,
    public WebGLZeroingObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLTEXTURE_PRIVATE_IID)

    WebGLTexture(WebGLContext *context, WebGLuint name) :
        WebGLContextBoundObject(context),
        mDeleted(PR_FALSE), mHasEverBeenBound(PR_FALSE), mName(name),
        mTarget(0),
        mMinFilter(LOCAL_GL_NEAREST_MIPMAP_LINEAR),
        mMagFilter(LOCAL_GL_LINEAR),
        mWrapS(LOCAL_GL_REPEAT),
        mWrapT(LOCAL_GL_REPEAT),
        mFacesCount(0),
        mMaxLevelWithCustomImages(0),
        mHaveGeneratedMipmap(PR_FALSE),
        mFakeBlackStatus(DoNotNeedFakeBlack)
    {
    }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }

    PRBool Deleted() { return mDeleted; }
    PRBool HasEverBeenBound() { return mHasEverBeenBound; }
    void SetHasEverBeenBound(PRBool x) { mHasEverBeenBound = x; }
    WebGLuint GLName() { return mName; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLTEXTURE

protected:
    friend class WebGLContext;
    friend class WebGLFramebuffer;

    PRBool mDeleted;
    PRBool mHasEverBeenBound;
    WebGLuint mName;

    // we store information about the various images that are part of
    // this texture (cubemap faces, mipmap levels)

public:

    struct ImageInfo {
        ImageInfo() : mWidth(0), mHeight(0), mFormat(0), mType(0), mIsDefined(PR_FALSE) {}
        ImageInfo(WebGLsizei width, WebGLsizei height,
                  WebGLenum format, WebGLenum type)
            : mWidth(width), mHeight(height), mFormat(format), mType(type), mIsDefined(PR_TRUE) {}

        PRBool operator==(const ImageInfo& a) const {
            return mWidth == a.mWidth && mHeight == a.mHeight &&
                   mFormat == a.mFormat && mType == a.mType;
        }
        PRBool operator!=(const ImageInfo& a) const {
            return !(*this == a);
        }
        PRBool IsSquare() const {
            return mWidth == mHeight;
        }
        PRBool IsPositive() const {
            return mWidth > 0 && mHeight > 0;
        }
        PRBool IsPowerOfTwo() const {
            return is_pot_assuming_nonnegative(mWidth) &&
                   is_pot_assuming_nonnegative(mHeight); // negative sizes should never happen (caught in texImage2D...)
        }
        PRInt64 MemoryUsage() const {
            if (!mIsDefined)
                return 0;
            PRInt64 texelSize = WebGLContext::GetTexelSize(mFormat, mType);
            return PRInt64(mWidth) * PRInt64(mHeight) * texelSize;
        }
        WebGLsizei mWidth, mHeight;
        WebGLenum mFormat, mType;
        PRBool mIsDefined;
    };

    ImageInfo& ImageInfoAt(size_t level, size_t face = 0) {
#ifdef DEBUG
        if (face >= mFacesCount)
            NS_ERROR("wrong face index, must be 0 for TEXTURE_2D and at most 5 for cube maps");
#endif
        // no need to check level as a wrong value would be caught by ElementAt().
        return mImageInfos.ElementAt(level * mFacesCount + face);
    }

    const ImageInfo& ImageInfoAt(size_t level, size_t face) const {
        return const_cast<WebGLTexture*>(this)->ImageInfoAt(level, face);
    }

    PRBool HasImageInfoAt(size_t level, size_t face) const {
        return level <= mMaxLevelWithCustomImages &&
               face < mFacesCount &&
               ImageInfoAt(level, 0).mIsDefined;
    }

    static size_t FaceForTarget(WebGLenum target) {
        return target == LOCAL_GL_TEXTURE_2D ? 0 : target - LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }

    PRInt64 MemoryUsage() const {
        PRInt64 result = 0;
        for(size_t face = 0; face < mFacesCount; face++) {
            if (mHaveGeneratedMipmap) {
                // Each mipmap level is 1/4 the size of the previous level
                // 1 + x + x^2 + ... = 1/(1-x)
                // for x = 1/4, we get 1/(1-1/4) = 4/3
                result += ImageInfoAt(0, face).MemoryUsage() * 4 / 3;
            } else {
                for(size_t level = 0; level <= mMaxLevelWithCustomImages; level++)
                    result += ImageInfoAt(level, face).MemoryUsage();
            }
        }
        return result;
    }

protected:

    WebGLenum mTarget;
    WebGLenum mMinFilter, mMagFilter, mWrapS, mWrapT;

    size_t mFacesCount, mMaxLevelWithCustomImages;
    nsTArray<ImageInfo> mImageInfos;

    PRBool mHaveGeneratedMipmap;
    FakeBlackStatus mFakeBlackStatus;

    void EnsureMaxLevelWithCustomImagesAtLeast(size_t aMaxLevelWithCustomImages) {
        mMaxLevelWithCustomImages = NS_MAX(mMaxLevelWithCustomImages, aMaxLevelWithCustomImages);
        mImageInfos.EnsureLengthAtLeast((mMaxLevelWithCustomImages + 1) * mFacesCount);
    }

    PRBool CheckFloatTextureFilterParams() const {
        // Without OES_texture_float_linear, only NEAREST and NEAREST_MIMPAMP_NEAREST are supported
        return (mMagFilter == LOCAL_GL_NEAREST) &&
            (mMinFilter == LOCAL_GL_NEAREST || mMinFilter == LOCAL_GL_NEAREST_MIPMAP_NEAREST);
    }

    PRBool DoesMinFilterRequireMipmap() const {
        return !(mMinFilter == LOCAL_GL_NEAREST || mMinFilter == LOCAL_GL_LINEAR);
    }

    PRBool AreBothWrapModesClampToEdge() const {
        return mWrapS == LOCAL_GL_CLAMP_TO_EDGE && mWrapT == LOCAL_GL_CLAMP_TO_EDGE;
    }

    PRBool DoesTexture2DMipmapHaveAllLevelsConsistentlyDefined(size_t face) const {
        if (mHaveGeneratedMipmap)
            return PR_TRUE;

        ImageInfo expected = ImageInfoAt(0, face);

        // checks if custom level>0 images are all defined up to the highest level defined
        // and have the expected dimensions
        for (size_t level = 0; level <= mMaxLevelWithCustomImages; ++level) {
            const ImageInfo& actual = ImageInfoAt(level, face);
            if (actual != expected)
                return PR_FALSE;
            expected.mWidth = NS_MAX(1, expected.mWidth >> 1);
            expected.mHeight = NS_MAX(1, expected.mHeight >> 1);

            // if the current level has size 1x1, we can stop here: the spec doesn't seem to forbid the existence
            // of extra useless levels.
            if (actual.mWidth == 1 && actual.mHeight == 1)
                return PR_TRUE;
        }

        // if we're here, we've exhausted all levels without finding a 1x1 image
        return PR_FALSE;
    }

public:

    void SetDontKnowIfNeedFakeBlack() {
        mFakeBlackStatus = DontKnowIfNeedFakeBlack;
        mContext->SetDontKnowIfNeedFakeBlack();
    }

    void Bind(WebGLenum aTarget) {
        // this function should only be called by bindTexture().
        // it assumes that the GL context is already current.

        PRBool firstTimeThisTextureIsBound = !mHasEverBeenBound;

        if (!firstTimeThisTextureIsBound && aTarget != mTarget) {
            mContext->ErrorInvalidOperation("bindTexture: this texture has already been bound to a different target");
            // very important to return here before modifying texture state! This was the place when I lost a whole day figuring
            // very strange 'invalid write' crashes.
            return;
        }

        mTarget = aTarget;

        mContext->gl->fBindTexture(mTarget, mName);

        if (firstTimeThisTextureIsBound) {
            mFacesCount = (mTarget == LOCAL_GL_TEXTURE_2D) ? 1 : 6;
            EnsureMaxLevelWithCustomImagesAtLeast(0);
            SetDontKnowIfNeedFakeBlack();

            // thanks to the WebKit people for finding this out: GL_TEXTURE_WRAP_R is not
            // present in GLES 2, but is present in GL and it seems as if for cube maps
            // we need to set it to GL_CLAMP_TO_EDGE to get the expected GLES behavior.
            if (mTarget == LOCAL_GL_TEXTURE_CUBE_MAP && !mContext->gl->IsGLES2())
                mContext->gl->fTexParameteri(mTarget, LOCAL_GL_TEXTURE_WRAP_R, LOCAL_GL_CLAMP_TO_EDGE);
        }

        mHasEverBeenBound = PR_TRUE;
    }

    void SetImageInfo(WebGLenum aTarget, WebGLint aLevel,
                      WebGLsizei aWidth, WebGLsizei aHeight,
                      WebGLenum aFormat, WebGLenum aType)
    {
        if ( (aTarget == LOCAL_GL_TEXTURE_2D) != (mTarget == LOCAL_GL_TEXTURE_2D) )
            return;

        size_t face = FaceForTarget(aTarget);

        EnsureMaxLevelWithCustomImagesAtLeast(aLevel);

        ImageInfoAt(aLevel, face) = ImageInfo(aWidth, aHeight, aFormat, aType);

        if (aLevel > 0)
            SetCustomMipmap();

        SetDontKnowIfNeedFakeBlack();
    }

    void SetMinFilter(WebGLenum aMinFilter) {
        mMinFilter = aMinFilter;
        SetDontKnowIfNeedFakeBlack();
    }
    void SetMagFilter(WebGLenum aMagFilter) {
        mMagFilter = aMagFilter;
        SetDontKnowIfNeedFakeBlack();
    }
    void SetWrapS(WebGLenum aWrapS) {
        mWrapS = aWrapS;
        SetDontKnowIfNeedFakeBlack();
    }
    void SetWrapT(WebGLenum aWrapT) {
        mWrapT = aWrapT;
        SetDontKnowIfNeedFakeBlack();
    }

    void SetGeneratedMipmap() {
        if (!mHaveGeneratedMipmap) {
            mHaveGeneratedMipmap = PR_TRUE;
            SetDontKnowIfNeedFakeBlack();
        }
    }

    void SetCustomMipmap() {
        if (mHaveGeneratedMipmap) {
            // if we were in GeneratedMipmap mode and are now switching to CustomMipmap mode,
            // we need to compute now all the mipmap image info.

            // since we were in GeneratedMipmap mode, we know that the level 0 images all have the same info,
            // and are power-of-two.
            ImageInfo imageInfo = ImageInfoAt(0, 0);
            NS_ASSERTION(imageInfo.IsPowerOfTwo(), "this texture is NPOT, so how could GenerateMipmap() ever accept it?");

            WebGLsizei size = NS_MAX(imageInfo.mWidth, imageInfo.mHeight);

            // so, the size is a power of two, let's find its log in base 2.
            size_t maxLevel = 0;
            for (WebGLsizei n = size; n > 1; n >>= 1)
                ++maxLevel;

            EnsureMaxLevelWithCustomImagesAtLeast(maxLevel);

            for (size_t level = 1; level <= maxLevel; ++level) {
                // again, since the sizes are powers of two, no need for any max(1,x) computation
                imageInfo.mWidth >>= 1;
                imageInfo.mHeight >>= 1;
                for(size_t face = 0; face < mFacesCount; ++face)
                    ImageInfoAt(level, face) = imageInfo;
            }
        }
        mHaveGeneratedMipmap = PR_FALSE;
    }

    PRBool IsFirstImagePowerOfTwo() const {
        return ImageInfoAt(0, 0).IsPowerOfTwo();
    }

    PRBool AreAllLevel0ImageInfosEqual() const {
        for (size_t face = 1; face < mFacesCount; ++face) {
            if (ImageInfoAt(0, face) != ImageInfoAt(0, 0))
                return PR_FALSE;
        }
        return PR_TRUE;
    }

    PRBool IsMipmapTexture2DComplete() const {
        if (mTarget != LOCAL_GL_TEXTURE_2D)
            return PR_FALSE;
        if (!ImageInfoAt(0, 0).IsPositive())
            return PR_FALSE;
        if (mHaveGeneratedMipmap)
            return PR_TRUE;
        return DoesTexture2DMipmapHaveAllLevelsConsistentlyDefined(0);
    }

    PRBool IsCubeComplete() const {
        if (mTarget != LOCAL_GL_TEXTURE_CUBE_MAP)
            return PR_FALSE;
        const ImageInfo &first = ImageInfoAt(0, 0);
        if (!first.IsPositive() || !first.IsSquare())
            return PR_FALSE;
        return AreAllLevel0ImageInfosEqual();
    }

    PRBool IsMipmapCubeComplete() const {
        if (!IsCubeComplete()) // in particular, this checks that this is a cube map
            return PR_FALSE;
        for (size_t face = 0; face < mFacesCount; ++face) {
            if (!DoesTexture2DMipmapHaveAllLevelsConsistentlyDefined(face))
                return PR_FALSE;
        }
        return PR_TRUE;
    }

    PRBool NeedFakeBlack() {
        // handle this case first, it's the generic case
        if (mFakeBlackStatus == DoNotNeedFakeBlack)
            return PR_FALSE;

        if (mFakeBlackStatus == DontKnowIfNeedFakeBlack) {
            // Determine if the texture needs to be faked as a black texture.
            // See 3.8.2 Shader Execution in the OpenGL ES 2.0.24 spec.

            for (size_t face = 0; face < mFacesCount; ++face) {
                if (!ImageInfoAt(0, face).mIsDefined) {
                    // In case of undefined texture image, we don't print any message because this is a very common
                    // and often legitimate case, for example when doing asynchronous texture loading.
                    // An extreme case of this is the photowall google demo.
                    // Exiting early here allows us to avoid making noise on valid webgl code.
                    mFakeBlackStatus = DoNeedFakeBlack;
                    return PR_TRUE;
                }
            }

            const char *msg_rendering_as_black
                = "A texture is going to be rendered as if it were black, as per the OpenGL ES 2.0.24 spec section 3.8.2, "
                  "because it";

            if (mTarget == LOCAL_GL_TEXTURE_2D)
            {
                if (DoesMinFilterRequireMipmap())
                {
                    if (!IsMipmapTexture2DComplete()) {
                        mContext->LogMessageIfVerbose
                            ("%s is a 2D texture, with a minification filter requiring a mipmap, "
                             "and is not mipmap complete (as defined in section 3.7.10).", msg_rendering_as_black);
                        mFakeBlackStatus = DoNeedFakeBlack;
                    } else if (!ImageInfoAt(0).IsPowerOfTwo()) {
                        mContext->LogMessageIfVerbose
                            ("%s is a 2D texture, with a minification filter requiring a mipmap, "
                             "and either its width or height is not a power of two.", msg_rendering_as_black);
                        mFakeBlackStatus = DoNeedFakeBlack;
                    }
                }
                else // no mipmap required
                {
                    if (!ImageInfoAt(0).IsPositive()) {
                        mContext->LogMessageIfVerbose
                            ("%s is a 2D texture and its width or height is equal to zero.",
                             msg_rendering_as_black);
                        mFakeBlackStatus = DoNeedFakeBlack;
                    } else if (!AreBothWrapModesClampToEdge() && !ImageInfoAt(0).IsPowerOfTwo()) {
                        mContext->LogMessageIfVerbose
                            ("%s is a 2D texture, with a minification filter not requiring a mipmap, "
                             "with its width or height not a power of two, and with a wrap mode "
                             "different from CLAMP_TO_EDGE.", msg_rendering_as_black);
                        mFakeBlackStatus = DoNeedFakeBlack;
                    }
                }
            }
            else // cube map
            {
                PRBool areAllLevel0ImagesPOT = PR_TRUE;
                for (size_t face = 0; face < mFacesCount; ++face)
                    areAllLevel0ImagesPOT &= ImageInfoAt(0, face).IsPowerOfTwo();

                if (DoesMinFilterRequireMipmap())
                {
                    if (!IsMipmapCubeComplete()) {
                        mContext->LogMessageIfVerbose("%s is a cube map texture, with a minification filter requiring a mipmap, "
                                   "and is not mipmap cube complete (as defined in section 3.7.10).",
                                   msg_rendering_as_black);
                        mFakeBlackStatus = DoNeedFakeBlack;
                    } else if (!areAllLevel0ImagesPOT) {
                        mContext->LogMessageIfVerbose("%s is a cube map texture, with a minification filter requiring a mipmap, "
                                   "and either the width or the height of some level 0 image is not a power of two.",
                                   msg_rendering_as_black);
                        mFakeBlackStatus = DoNeedFakeBlack;
                    }
                }
                else // no mipmap required
                {
                    if (!IsCubeComplete()) {
                        mContext->LogMessageIfVerbose("%s is a cube map texture, with a minification filter not requiring a mipmap, "
                                   "and is not cube complete (as defined in section 3.7.10).",
                                   msg_rendering_as_black);
                        mFakeBlackStatus = DoNeedFakeBlack;
                    } else if (!AreBothWrapModesClampToEdge() && !areAllLevel0ImagesPOT) {
                        mContext->LogMessageIfVerbose("%s is a cube map texture, with a minification filter not requiring a mipmap, "
                                   "with some level 0 image having width or height not a power of two, and with a wrap mode "
                                   "different from CLAMP_TO_EDGE.", msg_rendering_as_black);
                        mFakeBlackStatus = DoNeedFakeBlack;
                    }
                }
            }

            // we have exhausted all cases where we do need fakeblack, so if the status is still unknown,
            // that means that we do NOT need it.
            if (mFakeBlackStatus == DontKnowIfNeedFakeBlack)
                mFakeBlackStatus = DoNotNeedFakeBlack;
        }

        return mFakeBlackStatus == DoNeedFakeBlack;
    }
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLTexture, WEBGLTEXTURE_PRIVATE_IID)

#define WEBGLSHADER_PRIVATE_IID \
    {0x48cce975, 0xd459, 0x4689, {0x83, 0x82, 0x37, 0x82, 0x6e, 0xac, 0xe0, 0xa7}}
class WebGLShader :
    public nsIWebGLShader,
    public WebGLZeroingObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLSHADER_PRIVATE_IID)

    WebGLShader(WebGLContext *context, WebGLuint name, WebGLenum stype) :
        WebGLContextBoundObject(context),
        mName(name), mDeleted(PR_FALSE), mType(stype),
        mNeedsTranslation(true), mAttachCount(0)
    { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }

    PRBool Deleted() { return mDeleted && mAttachCount == 0; }
    WebGLuint GLName() { return mName; }
    WebGLenum ShaderType() { return mType; }

    PRUint32 AttachCount() { return mAttachCount; }
    void IncrementAttachCount() { mAttachCount++; }
    void DecrementAttachCount() { mAttachCount--; }

    void SetSource(const nsCString& src) {
        // XXX do some quick gzip here maybe -- getting this will be very rare
        mSource.Assign(src);
    }

    const nsCString& Source() const { return mSource; }

    void SetNeedsTranslation() { mNeedsTranslation = true; }
    bool NeedsTranslation() const { return mNeedsTranslation; }

    void SetTranslationSuccess() {
        mTranslationLog.SetIsVoid(PR_TRUE);
        mNeedsTranslation = false;
    }

    void SetTranslationFailure(const nsCString& msg) {
        mTranslationLog.Assign(msg);
    }

    const nsCString& TranslationLog() const { return mTranslationLog; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLSHADER
protected:
    WebGLuint mName;
    PRBool mDeleted;
    WebGLenum mType;
    nsCString mSource;
    nsCString mTranslationLog;
    bool mNeedsTranslation;
    PRUint32 mAttachCount;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLShader, WEBGLSHADER_PRIVATE_IID)

#define WEBGLPROGRAM_PRIVATE_IID \
    {0xb3084a5b, 0xa5b4, 0x4ee0, {0xa0, 0xf0, 0xfb, 0xdd, 0x64, 0xaf, 0x8e, 0x82}}
class WebGLProgram :
    public nsIWebGLProgram,
    public WebGLZeroingObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLPROGRAM_PRIVATE_IID)

    WebGLProgram(WebGLContext *context, WebGLuint name) :
        WebGLContextBoundObject(context),
        mName(name), mDeleted(PR_FALSE), mDeletePending(PR_FALSE),
        mLinkStatus(PR_FALSE), mGeneration(0),
        mUniformMaxNameLength(0), mAttribMaxNameLength(0),
        mUniformCount(0), mAttribCount(0)
    {
        mMapUniformLocations.Init();
    }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }

    void DetachShaders() {
        for (PRUint32 i = 0; i < mAttachedShaders.Length(); ++i) {
            if (mAttachedShaders[i])
                mAttachedShaders[i]->DecrementAttachCount();
        }
        mAttachedShaders.Clear();
    }

    PRBool Deleted() { return mDeleted && !mDeletePending; }
    void SetDeletePending() { mDeletePending = PR_TRUE; }
    void ClearDeletePending() { mDeletePending = PR_FALSE; }
    PRBool HasDeletePending() { return mDeletePending; }

    WebGLuint GLName() { return mName; }
    const nsTArray<nsRefPtr<WebGLShader> >& AttachedShaders() const { return mAttachedShaders; }
    PRBool LinkStatus() { return mLinkStatus; }
    PRUint32 Generation() const { return mGeneration.value(); }
    void SetLinkStatus(PRBool val) { mLinkStatus = val; }

    PRBool ContainsShader(WebGLShader *shader) {
        return mAttachedShaders.Contains(shader);
    }

    // return true if the shader wasn't already attached
    PRBool AttachShader(WebGLShader *shader) {
        if (ContainsShader(shader))
            return PR_FALSE;
        mAttachedShaders.AppendElement(shader);
        shader->IncrementAttachCount();
        return PR_TRUE;
    }

    // return true if the shader was found and removed
    PRBool DetachShader(WebGLShader *shader) {
        if (mAttachedShaders.RemoveElement(shader)) {
            shader->DecrementAttachCount();
            return PR_TRUE;
        }
        return PR_FALSE;
    }

    PRBool HasAttachedShaderOfType(GLenum shaderType) {
        for (PRUint32 i = 0; i < mAttachedShaders.Length(); ++i) {
            if (mAttachedShaders[i] && mAttachedShaders[i]->ShaderType() == shaderType) {
                return PR_TRUE;
            }
        }
        return PR_FALSE;
    }

    PRBool HasBothShaderTypesAttached() {
        return
            HasAttachedShaderOfType(LOCAL_GL_VERTEX_SHADER) &&
            HasAttachedShaderOfType(LOCAL_GL_FRAGMENT_SHADER);
    }

    PRBool NextGeneration()
    {
        if (!(mGeneration+1).valid())
            return PR_FALSE; // must exit without changing mGeneration
        ++mGeneration;
        mMapUniformLocations.Clear();
        return PR_TRUE;
    }
    

    already_AddRefed<WebGLUniformLocation> GetUniformLocationObject(GLint glLocation);

    /* Called only after LinkProgram */
    PRBool UpdateInfo(gl::GLContext *gl);

    /* Getters for cached program info */
    WebGLint UniformMaxNameLength() const { return mUniformMaxNameLength; }
    WebGLint AttribMaxNameLength() const { return mAttribMaxNameLength; }
    WebGLint UniformCount() const { return mUniformCount; }
    WebGLint AttribCount() const { return mAttribCount; }
    bool IsAttribInUse(unsigned i) const { return mAttribsInUse[i]; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLPROGRAM
protected:
    WebGLuint mName;
    PRPackedBool mDeleted;
    PRPackedBool mDeletePending;
    PRPackedBool mLinkStatus;
    // attached shaders of the program object
    nsTArray<nsRefPtr<WebGLShader> > mAttachedShaders;
    CheckedUint32 mGeneration;

    // post-link data
    nsRefPtrHashtable<nsUint32HashKey, WebGLUniformLocation> mMapUniformLocations;
    GLint mUniformMaxNameLength;
    GLint mAttribMaxNameLength;
    GLint mUniformCount;
    GLint mAttribCount;
    std::vector<bool> mAttribsInUse;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLProgram, WEBGLPROGRAM_PRIVATE_IID)

#define WEBGLRENDERBUFFER_PRIVATE_IID \
    {0x3cbc2067, 0x5831, 0x4e3f, {0xac, 0x52, 0x7e, 0xf4, 0x5c, 0x04, 0xff, 0xae}}
class WebGLRenderbuffer :
    public nsIWebGLRenderbuffer,
    public WebGLZeroingObject,
    public WebGLRectangleObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLRENDERBUFFER_PRIVATE_IID)

    WebGLRenderbuffer(WebGLContext *context, WebGLuint name, WebGLuint secondBufferName = 0) :
        WebGLContextBoundObject(context),
        mName(name),
        mInternalFormat(0),
        mInternalFormatForGL(0),
        mDeleted(PR_FALSE), mHasEverBeenBound(PR_FALSE), mInitialized(PR_FALSE)
    { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }
    PRBool Deleted() const { return mDeleted; }
    PRBool HasEverBeenBound() { return mHasEverBeenBound; }
    void SetHasEverBeenBound(PRBool x) { mHasEverBeenBound = x; }
    WebGLuint GLName() const { return mName; }

    PRBool Initialized() const { return mInitialized; }
    void SetInitialized(PRBool aInitialized) { mInitialized = aInitialized; }

    WebGLenum InternalFormat() const { return mInternalFormat; }
    void SetInternalFormat(WebGLenum aInternalFormat) { mInternalFormat = aInternalFormat; }
    
    WebGLenum InternalFormatForGL() const { return mInternalFormatForGL; }
    void SetInternalFormatForGL(WebGLenum aInternalFormatForGL) { mInternalFormatForGL = aInternalFormatForGL; }
    
    PRInt64 MemoryUsage() const {
        PRInt64 pixels = PRInt64(width()) * PRInt64(height());
        switch (mInternalFormatForGL) {
            case LOCAL_GL_STENCIL_INDEX8:
                return pixels;
            case LOCAL_GL_RGBA4:
            case LOCAL_GL_RGB5_A1:
            case LOCAL_GL_RGB565:
            case LOCAL_GL_DEPTH_COMPONENT16:
                return 2 * pixels;
            case LOCAL_GL_RGB8:
            case LOCAL_GL_DEPTH_COMPONENT24:
                return 3*pixels;
            case LOCAL_GL_RGBA8:
            case LOCAL_GL_DEPTH24_STENCIL8:
                return 4*pixels;
            default:
                break;
        }
        NS_ABORT();
        return 0;
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLRENDERBUFFER

protected:
    WebGLuint mName;
    WebGLenum mInternalFormat;
    WebGLenum mInternalFormatForGL;

    PRBool mDeleted;
    PRBool mHasEverBeenBound;
    PRBool mInitialized;

    friend class WebGLFramebuffer;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLRenderbuffer, WEBGLRENDERBUFFER_PRIVATE_IID)

class WebGLFramebufferAttachment
    : public WebGLRectangleObject
{
    // deleting a texture or renderbuffer immediately detaches it
    WebGLObjectRefPtr<WebGLTexture> mTexturePtr;
    WebGLObjectRefPtr<WebGLRenderbuffer> mRenderbufferPtr;
    WebGLenum mAttachmentPoint;
    WebGLint mTextureLevel;
    WebGLenum mTextureCubeMapFace;

public:
    WebGLFramebufferAttachment(WebGLenum aAttachmentPoint)
        : mAttachmentPoint(aAttachmentPoint)
    {}

    PRBool IsNull() const {
        return !mTexturePtr && !mRenderbufferPtr;
    }

    PRBool HasAlpha() const {
        WebGLenum format = 0;
        if (mTexturePtr)
            format = mTexturePtr->ImageInfoAt(0,0).mFormat;
        if (mRenderbufferPtr)
            format = mRenderbufferPtr->InternalFormat();
        return format == LOCAL_GL_RGBA ||
               format == LOCAL_GL_LUMINANCE_ALPHA ||
               format == LOCAL_GL_ALPHA ||
               format == LOCAL_GL_RGBA4 ||
               format == LOCAL_GL_RGB5_A1;
    }

    void SetTexture(WebGLTexture *tex, WebGLint level, WebGLenum face) {
        mTexturePtr = tex;
        mRenderbufferPtr = nsnull;
        mTextureLevel = level;
        mTextureCubeMapFace = face;
        if (tex) {
            const WebGLTexture::ImageInfo &imageInfo = tex->ImageInfoAt(level, face);
            setDimensions(imageInfo.mWidth, imageInfo.mHeight);
        } else {
            setDimensions(0, 0);
        }
    }
    void SetRenderbuffer(WebGLRenderbuffer *rb) {
        mTexturePtr = nsnull;
        mRenderbufferPtr = rb;
        setDimensions(rb);
    }
    WebGLTexture *Texture() const {
        return mTexturePtr.get();
    }
    WebGLRenderbuffer *Renderbuffer() const {
        return mRenderbufferPtr.get();
    }
    WebGLint TextureLevel() const {
        return mTextureLevel;
    }
    WebGLenum TextureCubeMapFace() const {
        return mTextureCubeMapFace;
    }

    PRBool IsIncompatibleWithAttachmentPoint() const
    {
        // textures can only be color textures in WebGL
        if (mTexturePtr)
            return mAttachmentPoint != LOCAL_GL_COLOR_ATTACHMENT0;

        if (mRenderbufferPtr) {
            WebGLenum format = mRenderbufferPtr->InternalFormat();
            switch (mAttachmentPoint) {
                case LOCAL_GL_COLOR_ATTACHMENT0:
                    return format != LOCAL_GL_RGB565 &&
                           format != LOCAL_GL_RGB5_A1 &&
                           format != LOCAL_GL_RGBA4;
                case LOCAL_GL_DEPTH_ATTACHMENT:
                    return format != LOCAL_GL_DEPTH_COMPONENT16;
                case LOCAL_GL_STENCIL_ATTACHMENT:
                    return format != LOCAL_GL_STENCIL_INDEX8;
                case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
                    return format != LOCAL_GL_DEPTH_STENCIL;
            }
        }

        return PR_FALSE; // no attachment at all, so no incompatibility
    }

    PRBool HasUninitializedRenderbuffer() const {
        return mRenderbufferPtr && !mRenderbufferPtr->Initialized();
    }
};

#define WEBGLFRAMEBUFFER_PRIVATE_IID \
    {0x0052a16f, 0x4bc9, 0x4a55, {0x9d, 0xa3, 0x54, 0x95, 0xaa, 0x4e, 0x80, 0xb9}}
class WebGLFramebuffer :
    public nsIWebGLFramebuffer,
    public WebGLZeroingObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLFRAMEBUFFER_PRIVATE_IID)

    WebGLFramebuffer(WebGLContext *context, WebGLuint name) :
        WebGLContextBoundObject(context),
        mName(name), mDeleted(PR_FALSE), mHasEverBeenBound(PR_FALSE),
        mColorAttachment(LOCAL_GL_COLOR_ATTACHMENT0),
        mDepthAttachment(LOCAL_GL_DEPTH_ATTACHMENT),
        mStencilAttachment(LOCAL_GL_STENCIL_ATTACHMENT),
        mDepthStencilAttachment(LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
    { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }
    PRBool Deleted() { return mDeleted; }
    PRBool HasEverBeenBound() { return mHasEverBeenBound; }
    void SetHasEverBeenBound(PRBool x) { mHasEverBeenBound = x; }
    WebGLuint GLName() { return mName; }
    
    WebGLsizei width() { return mColorAttachment.width(); }
    WebGLsizei height() { return mColorAttachment.height(); }

    nsresult FramebufferRenderbuffer(WebGLenum target,
                                     WebGLenum attachment,
                                     WebGLenum rbtarget,
                                     nsIWebGLRenderbuffer *rbobj)
    {
        WebGLuint renderbuffername;
        PRBool isNull;
        WebGLRenderbuffer *wrb;

        if (!mContext->GetConcreteObjectAndGLName("framebufferRenderbuffer: renderbuffer",
                                                  rbobj, &wrb, &renderbuffername, &isNull))
        {
            return NS_OK;
        }

        if (target != LOCAL_GL_FRAMEBUFFER)
            return mContext->ErrorInvalidEnumInfo("framebufferRenderbuffer: target", target);

        if (rbtarget != LOCAL_GL_RENDERBUFFER)
            return mContext->ErrorInvalidEnumInfo("framebufferRenderbuffer: renderbuffer target:", rbtarget);

        switch (attachment) {
        case LOCAL_GL_DEPTH_ATTACHMENT:
            mDepthAttachment.SetRenderbuffer(wrb);
            break;
        case LOCAL_GL_STENCIL_ATTACHMENT:
            mStencilAttachment.SetRenderbuffer(wrb);
            break;
        case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
            mDepthStencilAttachment.SetRenderbuffer(wrb);
            break;
        default:
            // finish checking that the 'attachment' parameter is among the allowed values
            if (attachment != LOCAL_GL_COLOR_ATTACHMENT0)
                return mContext->ErrorInvalidEnumInfo("framebufferRenderbuffer: attachment", attachment);

            mColorAttachment.SetRenderbuffer(wrb);
            break;
        }

        mContext->MakeContextCurrent();
        if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
            mContext->gl->fFramebufferRenderbuffer(target, LOCAL_GL_DEPTH_ATTACHMENT, rbtarget, renderbuffername);
            mContext->gl->fFramebufferRenderbuffer(target, LOCAL_GL_STENCIL_ATTACHMENT, rbtarget, renderbuffername);
        } else {
            mContext->gl->fFramebufferRenderbuffer(target, attachment, rbtarget, renderbuffername);
        }

        return NS_OK;
    }

    nsresult FramebufferTexture2D(WebGLenum target,
                                  WebGLenum attachment,
                                  WebGLenum textarget,
                                  nsIWebGLTexture *tobj,
                                  WebGLint level)
    {
        WebGLuint texturename;
        PRBool isNull;
        WebGLTexture *wtex;

        if (!mContext->GetConcreteObjectAndGLName("framebufferTexture2D: texture",
                                                  tobj, &wtex, &texturename, &isNull))
        {
            return NS_OK;
        }

        if (target != LOCAL_GL_FRAMEBUFFER)
            return mContext->ErrorInvalidEnumInfo("framebufferTexture2D: target", target);

        if (textarget != LOCAL_GL_TEXTURE_2D &&
            (textarget < LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
             textarget > LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z))
            return mContext->ErrorInvalidEnumInfo("framebufferTexture2D: invalid texture target", textarget);

        if (level != 0)
            return mContext->ErrorInvalidValue("framebufferTexture2D: level must be 0");

        size_t face = WebGLTexture::FaceForTarget(textarget);
        switch (attachment) {
        case LOCAL_GL_DEPTH_ATTACHMENT:
            mDepthAttachment.SetTexture(wtex, level, face);
            break;
        case LOCAL_GL_STENCIL_ATTACHMENT:
            mStencilAttachment.SetTexture(wtex, level, face);
            break;
        case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
            mDepthStencilAttachment.SetTexture(wtex, level, face);
            break;
        default:
            if (attachment != LOCAL_GL_COLOR_ATTACHMENT0)
                return mContext->ErrorInvalidEnumInfo("framebufferTexture2D: attachment", attachment);

            mColorAttachment.SetTexture(wtex, level, face);
            break;
        }

        mContext->MakeContextCurrent();
        if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
            mContext->gl->fFramebufferTexture2D(target, LOCAL_GL_DEPTH_ATTACHMENT, textarget, texturename, level);
            mContext->gl->fFramebufferTexture2D(target, LOCAL_GL_STENCIL_ATTACHMENT, textarget, texturename, level);
        } else {
            mContext->gl->fFramebufferTexture2D(target, attachment, textarget, texturename, level);
        }

        return NS_OK;
    }

    PRBool CheckAndInitializeRenderbuffers()
    {
        if (HasBadAttachments()) {
            mContext->SynthesizeGLError(LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION);
            return PR_FALSE;
        }

        if (mColorAttachment.HasUninitializedRenderbuffer() ||
            mDepthAttachment.HasUninitializedRenderbuffer() ||
            mStencilAttachment.HasUninitializedRenderbuffer() ||
            mDepthStencilAttachment.HasUninitializedRenderbuffer())
        {
            InitializeRenderbuffers();
        }

        return PR_TRUE;
    }

    PRBool HasBadAttachments() const {
        if (mColorAttachment.IsIncompatibleWithAttachmentPoint() ||
            mDepthAttachment.IsIncompatibleWithAttachmentPoint() ||
            mStencilAttachment.IsIncompatibleWithAttachmentPoint() ||
            mDepthStencilAttachment.IsIncompatibleWithAttachmentPoint())
        {
            // some attachment is incompatible with its attachment point
            return PR_TRUE;
        }
        
        if (int(mDepthAttachment.IsNull()) +
            int(mStencilAttachment.IsNull()) +
            int(mDepthStencilAttachment.IsNull()) <= 1)
        {
            // has at least two among Depth, Stencil, DepthStencil
            return PR_TRUE;
        }
        
        if (!mDepthAttachment.IsNull() && !mDepthAttachment.HasSameDimensionsAs(mColorAttachment))
            return PR_TRUE;
        if (!mStencilAttachment.IsNull() && !mStencilAttachment.HasSameDimensionsAs(mColorAttachment))
            return PR_TRUE;
        if (!mDepthStencilAttachment.IsNull() && !mDepthStencilAttachment.HasSameDimensionsAs(mColorAttachment))
            return PR_TRUE;
        
        else return PR_FALSE;
    }

    const WebGLFramebufferAttachment& ColorAttachment() const {
        return mColorAttachment;
    }

    const WebGLFramebufferAttachment& DepthAttachment() const {
        return mDepthAttachment;
    }

    const WebGLFramebufferAttachment& StencilAttachment() const {
        return mStencilAttachment;
    }

    const WebGLFramebufferAttachment& DepthStencilAttachment() const {
        return mDepthStencilAttachment;
    }

    const WebGLFramebufferAttachment& GetAttachment(WebGLenum attachment) const {
        if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
            return mDepthStencilAttachment;
        if (attachment == LOCAL_GL_DEPTH_ATTACHMENT)
            return mDepthAttachment;
        if (attachment == LOCAL_GL_STENCIL_ATTACHMENT)
            return mStencilAttachment;

        NS_ASSERTION(attachment == LOCAL_GL_COLOR_ATTACHMENT0, "bad attachment!");
        return mColorAttachment;
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLFRAMEBUFFER

protected:

    // protected because WebGLContext should only call InitializeRenderbuffers
    void InitializeRenderbuffers()
    {
        mContext->MakeContextCurrent();

        if (mContext->gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) != LOCAL_GL_FRAMEBUFFER_COMPLETE)
            return;

        PRUint32 mask = 0;

        if (mColorAttachment.HasUninitializedRenderbuffer())
            mask |= LOCAL_GL_COLOR_BUFFER_BIT;

        if (mDepthAttachment.HasUninitializedRenderbuffer() ||
            mDepthStencilAttachment.HasUninitializedRenderbuffer())
        {
            mask |= LOCAL_GL_DEPTH_BUFFER_BIT;
        }

        if (mStencilAttachment.HasUninitializedRenderbuffer() ||
            mDepthStencilAttachment.HasUninitializedRenderbuffer())
        {
            mask |= LOCAL_GL_STENCIL_BUFFER_BIT;
        }

        // the one useful line of code
        mContext->ForceClearFramebufferWithDefaultValues(mask, nsIntRect(0,0,width(),height()));

        if (mColorAttachment.HasUninitializedRenderbuffer())
            mColorAttachment.Renderbuffer()->SetInitialized(PR_TRUE);

        if (mDepthAttachment.HasUninitializedRenderbuffer())
            mDepthAttachment.Renderbuffer()->SetInitialized(PR_TRUE);

        if (mStencilAttachment.HasUninitializedRenderbuffer())
            mStencilAttachment.Renderbuffer()->SetInitialized(PR_TRUE);

        if (mDepthStencilAttachment.HasUninitializedRenderbuffer())
            mDepthStencilAttachment.Renderbuffer()->SetInitialized(PR_TRUE);
    }

    WebGLuint mName;
    PRPackedBool mDeleted;
    PRBool mHasEverBeenBound;

    // we only store pointers to attached renderbuffers, not to attached textures, because
    // we will only need to initialize renderbuffers. Textures are already initialized.
    WebGLFramebufferAttachment mColorAttachment,
                               mDepthAttachment,
                               mStencilAttachment,
                               mDepthStencilAttachment;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLFramebuffer, WEBGLFRAMEBUFFER_PRIVATE_IID)

#define WEBGLUNIFORMLOCATION_PRIVATE_IID \
    {0x01a8a614, 0xb109, 0x42f1, {0xb4, 0x40, 0x8d, 0x8b, 0x87, 0x0b, 0x43, 0xa7}}
class WebGLUniformLocation :
    public nsIWebGLUniformLocation,
    public WebGLZeroingObject,
    public WebGLContextBoundObject
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLUNIFORMLOCATION_PRIVATE_IID)

    WebGLUniformLocation(WebGLContext *context, WebGLProgram *program, GLint location) :
        WebGLContextBoundObject(context), mProgram(program), mProgramGeneration(program->Generation()),
        mLocation(location) { }

    WebGLProgram *Program() const { return mProgram; }
    GLint Location() const { return mLocation; }
    PRUint32 ProgramGeneration() const { return mProgramGeneration; }

    // needed for our generic helpers to check nsIxxx parameters, see GetConcreteObject.
    PRBool Deleted() { return PR_FALSE; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLUNIFORMLOCATION
protected:
    WebGLObjectRefPtr<WebGLProgram> mProgram;
    PRUint32 mProgramGeneration;
    GLint mLocation;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLUniformLocation, WEBGLUNIFORMLOCATION_PRIVATE_IID)

#define WEBGLACTIVEINFO_PRIVATE_IID \
    {0x90def5ec, 0xc672, 0x4ac3, {0xb8, 0x97, 0x04, 0xa2, 0x6d, 0xda, 0x66, 0xd7}}
class WebGLActiveInfo :
    public nsIWebGLActiveInfo
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLACTIVEINFO_PRIVATE_IID)

    WebGLActiveInfo(WebGLint size, WebGLenum type, const char *nameptr, PRUint32 namelength) :
        mDeleted(PR_FALSE),
        mSize(size),
        mType(type)
    {
        mName.AssignASCII(nameptr, namelength);
    }

    void Delete() {
        if (mDeleted)
            return;
        mDeleted = PR_TRUE;
    }

    PRBool Deleted() { return mDeleted; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLACTIVEINFO
protected:
    PRBool mDeleted;
    WebGLint mSize;
    WebGLenum mType;
    nsString mName;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLActiveInfo, WEBGLACTIVEINFO_PRIVATE_IID)

#define WEBGLEXTENSION_PRIVATE_IID \
    {0x457dd0b2, 0x9f77, 0x4c23, {0x95, 0x70, 0x9d, 0x62, 0x65, 0xc1, 0xa4, 0x81}}
class WebGLExtension :
    public nsIWebGLExtension,
    public WebGLContextBoundObject,
    public WebGLZeroingObject
{
public:
    WebGLExtension(WebGLContext *baseContext)
        : WebGLContextBoundObject(baseContext)
    {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLEXTENSION

    NS_DECLARE_STATIC_IID_ACCESSOR(WEBGLEXTENSION_PRIVATE_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebGLExtension, WEBGLACTIVEINFO_PRIVATE_IID)

/**
 ** Template implementations
 **/

/* Helper function taking a BaseInterfaceType pointer and check that
 * it matches the required concrete implementation type (if it's
 * non-null), that it's not null/deleted unless we allowed it to, and
 * obtain a pointer to the concrete object.
 *
 * By default, null (respectively: deleted) aInterface pointers are
 * not allowed, but if you pass a non-null isNull (respectively:
 * isDeleted) pointer, then they become allowed and the value at
 * isNull (respecively isDeleted) is overwritten.
 *
 * If generateErrors is true (which is the default) then upon errors,
 * GL errors are synthesized and error messages are printed, prepended by
 * the 'info' string.
 */

template<class ConcreteObjectType, class BaseInterfaceType>
inline PRBool
WebGLContext::GetConcreteObject(const char *info,
                                BaseInterfaceType *aInterface,
                                ConcreteObjectType **aConcreteObject,
                                PRBool *isNull,
                                PRBool *isDeleted,
                                PRBool generateErrors)
{
    if (!aInterface) {
        if (NS_LIKELY(isNull)) {
            // non-null isNull means that the caller will accept a null arg
            *isNull = PR_TRUE;
            if(isDeleted) *isDeleted = PR_FALSE;
            *aConcreteObject = 0;
            return PR_TRUE;
        } else {
            if (generateErrors)
                ErrorInvalidValue("%s: null object passed as argument", info);
            return PR_FALSE;
        }
    }

    if (isNull)
        *isNull = PR_FALSE;

    nsresult rv;
    nsCOMPtr<ConcreteObjectType> tmp(do_QueryInterface(aInterface, &rv));
    if (NS_FAILED(rv))
        return PR_FALSE;

    *aConcreteObject = tmp;

    if (!(*aConcreteObject)->IsCompatibleWithContext(this)) {
        // the object doesn't belong to this WebGLContext
        if (generateErrors)
            ErrorInvalidOperation("%s: object from different WebGL context (or older generation of this one) "
                                  "passed as argument", info);
        return PR_FALSE;
    }

    if ((*aConcreteObject)->Deleted()) {
        if (NS_LIKELY(isDeleted)) {
            // non-null isDeleted means that the caller will accept a deleted arg
            *isDeleted = PR_TRUE;
            return PR_TRUE;
        } else {
            if (generateErrors)
                ErrorInvalidValue("%s: deleted object passed as argument", info);
            return PR_FALSE;
        }
    }

    if (isDeleted)
      *isDeleted = PR_FALSE;

    return PR_TRUE;
}

/* Same as GetConcreteObject, and in addition gets the GL object name.
 * Null objects give the name 0.
 */
template<class ConcreteObjectType, class BaseInterfaceType>
inline PRBool
WebGLContext::GetConcreteObjectAndGLName(const char *info,
                                         BaseInterfaceType *aInterface,
                                         ConcreteObjectType **aConcreteObject,
                                         WebGLuint *aGLObjectName,
                                         PRBool *isNull,
                                         PRBool *isDeleted)
{
    PRBool result = GetConcreteObject(info, aInterface, aConcreteObject, isNull, isDeleted);
    if (result == PR_FALSE) return PR_FALSE;
    *aGLObjectName = *aConcreteObject ? (*aConcreteObject)->GLName() : 0;
    return PR_TRUE;
}

/* Same as GetConcreteObjectAndGLName when you don't need the concrete object pointer.
 */
template<class ConcreteObjectType, class BaseInterfaceType>
inline PRBool
WebGLContext::GetGLName(const char *info,
                        BaseInterfaceType *aInterface,
                        WebGLuint *aGLObjectName,
                        PRBool *isNull,
                        PRBool *isDeleted)
{
    ConcreteObjectType *aConcreteObject;
    return GetConcreteObjectAndGLName(info, aInterface, &aConcreteObject, aGLObjectName, isNull, isDeleted);
}

/* Same as GetConcreteObject when you only want to check if the conversion succeeds.
 */
template<class ConcreteObjectType, class BaseInterfaceType>
inline PRBool
WebGLContext::CanGetConcreteObject(const char *info,
                              BaseInterfaceType *aInterface,
                              PRBool *isNull,
                              PRBool *isDeleted)
{
    ConcreteObjectType *aConcreteObject;
    return GetConcreteObject(info, aInterface, &aConcreteObject, isNull, isDeleted, PR_FALSE);
}

class WebGLMemoryReporter
{
    WebGLMemoryReporter();
    ~WebGLMemoryReporter();
    static WebGLMemoryReporter* sUniqueInstance;

    // here we store plain pointers, not RefPtrs: we don't want the WebGLMemoryReporter unique instance to keep alive all
    // WebGLContexts ever created.
    typedef nsTArray<const WebGLContext*> ContextsArrayType;
    ContextsArrayType mContexts;
    
    nsCOMPtr<nsIMemoryReporter> mTextureMemoryUsageReporter;
    nsCOMPtr<nsIMemoryReporter> mTextureCountReporter;
    nsCOMPtr<nsIMemoryReporter> mBufferMemoryUsageReporter;
    nsCOMPtr<nsIMemoryReporter> mBufferCacheMemoryUsageReporter;
    nsCOMPtr<nsIMemoryReporter> mBufferCountReporter;
    nsCOMPtr<nsIMemoryReporter> mRenderbufferMemoryUsageReporter;
    nsCOMPtr<nsIMemoryReporter> mRenderbufferCountReporter;
    nsCOMPtr<nsIMemoryReporter> mShaderSourcesSizeReporter;
    nsCOMPtr<nsIMemoryReporter> mShaderTranslationLogsSizeReporter;
    nsCOMPtr<nsIMemoryReporter> mShaderCountReporter;
    nsCOMPtr<nsIMemoryReporter> mContextCountReporter;

    static WebGLMemoryReporter* UniqueInstance();

    static ContextsArrayType & Contexts() { return UniqueInstance()->mContexts; }

  public:

    static void AddWebGLContext(const WebGLContext* c) {
        Contexts().AppendElement(c);
    }

    static void RemoveWebGLContext(const WebGLContext* c) {
        ContextsArrayType & contexts = Contexts();
        contexts.RemoveElement(c);
        if (contexts.IsEmpty()) {
            delete sUniqueInstance;
            sUniqueInstance = nsnull;
        }
    }

    static PLDHashOperator TextureMemoryUsageFunction(const PRUint32&, WebGLTexture *aValue, void *aData)
    {
        PRInt64 *result = (PRInt64*) aData;
        *result += aValue->MemoryUsage();
        return PL_DHASH_NEXT;
    }

    static PRInt64 GetTextureMemoryUsed() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            PRInt64 textureMemoryUsageForThisContext = 0;
            contexts[i]->mMapTextures.EnumerateRead(TextureMemoryUsageFunction, &textureMemoryUsageForThisContext);
            result += textureMemoryUsageForThisContext;
        }
        return result;
    }
    
    static PRInt64 GetTextureCount() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            result += contexts[i]->mMapTextures.Count();
        }
        return result;
    }
    
    static PLDHashOperator BufferMemoryUsageFunction(const PRUint32&, WebGLBuffer *aValue, void *aData)
    {
        PRInt64 *result = (PRInt64*) aData;
        *result += aValue->ByteLength();
        return PL_DHASH_NEXT;
    }

    static PRInt64 GetBufferMemoryUsed() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            PRInt64 bufferMemoryUsageForThisContext = 0;
            contexts[i]->mMapBuffers.EnumerateRead(BufferMemoryUsageFunction, &bufferMemoryUsageForThisContext);
            result += bufferMemoryUsageForThisContext;
        }
        return result;
    }
    
    static PLDHashOperator BufferCacheMemoryUsageFunction(const PRUint32&, WebGLBuffer *aValue, void *aData)
    {
        PRInt64 *result = (PRInt64*) aData;
        // element array buffers are cached in the WebGL implementation. Other buffers aren't.
        if (aValue->Target() == LOCAL_GL_ELEMENT_ARRAY_BUFFER)
          *result += aValue->ByteLength();
        return PL_DHASH_NEXT;
    }

    static PRInt64 GetBufferCacheMemoryUsed() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            PRInt64 bufferCacheMemoryUsageForThisContext = 0;
            contexts[i]->mMapBuffers.EnumerateRead(BufferCacheMemoryUsageFunction, &bufferCacheMemoryUsageForThisContext);
            result += bufferCacheMemoryUsageForThisContext;
        }
        return result;
    }

    static PRInt64 GetBufferCount() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            result += contexts[i]->mMapBuffers.Count();
        }
        return result;
    }
    
    static PLDHashOperator RenderbufferMemoryUsageFunction(const PRUint32&, WebGLRenderbuffer *aValue, void *aData)
    {
        PRInt64 *result = (PRInt64*) aData;
        *result += aValue->MemoryUsage();
        return PL_DHASH_NEXT;
    }

    static PRInt64 GetRenderbufferMemoryUsed() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            PRInt64 bufferMemoryUsageForThisContext = 0;
            contexts[i]->mMapRenderbuffers.EnumerateRead(RenderbufferMemoryUsageFunction, &bufferMemoryUsageForThisContext);
            result += bufferMemoryUsageForThisContext;
        }
        return result;
    }
    
    static PRInt64 GetRenderbufferCount() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            result += contexts[i]->mMapRenderbuffers.Count();
        }
        return result;
    }

    static PLDHashOperator ShaderSourceSizeFunction(const PRUint32&, WebGLShader *aValue, void *aData)
    {
        PRInt64 *result = (PRInt64*) aData;
        *result += aValue->Source().Length();
        return PL_DHASH_NEXT;
    }

    static PLDHashOperator ShaderTranslationLogSizeFunction(const PRUint32&, WebGLShader *aValue, void *aData)
    {
        PRInt64 *result = (PRInt64*) aData;
        *result += aValue->TranslationLog().Length();
        return PL_DHASH_NEXT;
    }

    static PRInt64 GetShaderSourcesSize() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            PRInt64 shaderSourcesSizeForThisContext = 0;
            contexts[i]->mMapShaders.EnumerateRead(ShaderSourceSizeFunction, &shaderSourcesSizeForThisContext);
            result += shaderSourcesSizeForThisContext;
        }
        return result;
    }
    
    static PRInt64 GetShaderTranslationLogsSize() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            PRInt64 shaderTranslationLogsSizeForThisContext = 0;
            contexts[i]->mMapShaders.EnumerateRead(ShaderTranslationLogSizeFunction, &shaderTranslationLogsSizeForThisContext);
            result += shaderTranslationLogsSizeForThisContext;
        }
        return result;
    }
    
    static PRInt64 GetShaderCount() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            result += contexts[i]->mMapShaders.Count();
        }
        return result;
    }

    static PRInt64 GetContextCount() {
        return Contexts().Length();
    }
};

}

#endif
