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
#include "nsIMemoryReporter.h"
#include "nsIJSNativeInitializer.h"
#include "nsContentUtils.h"

#include "GLContextProvider.h"
#include "Layers.h"

#include "CheckedInt.h"

#ifdef XP_MACOSX
#include "ForceDiscreteGPUHelperCGL.h"
#endif

/* 
 * Minimum value constants defined in 6.2 State Tables of OpenGL ES - 2.0.25
 *   https://bugzilla.mozilla.org/show_bug.cgi?id=686732
 * 
 * Exceptions: some of the following values are set to higher values than in the spec because
 * the values in the spec are ridiculously low. They are explicitly marked below
*/
#define MINVALUE_GL_MAX_TEXTURE_SIZE                  1024  // Different from the spec, which sets it to 64 on page 162
#define MINVALUE_GL_MAX_CUBE_MAP_TEXTURE_SIZE         512   // Different from the spec, which sets it to 16 on page 162
#define MINVALUE_GL_MAX_VERTEX_ATTRIBS                8     // Page 164
#define MINVALUE_GL_MAX_FRAGMENT_UNIFORM_VECTORS      16    // Page 164
#define MINVALUE_GL_MAX_VERTEX_UNIFORM_VECTORS        128   // Page 164
#define MINVALUE_GL_MAX_VARYING_VECTORS               8     // Page 164
#define MINVALUE_GL_MAX_TEXTURE_IMAGE_UNITS           8     // Page 164
#define MINVALUE_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS    0     // Page 164
#define MINVALUE_GL_MAX_RENDERBUFFER_SIZE             1024  // Different from the spec, which sets it to 1 on page 164
#define MINVALUE_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS  8     // Page 164

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
class WebGLExtension;
struct WebGLVertexAttribData;

class WebGLRectangleObject;
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

inline bool is_pot_assuming_nonnegative(WebGLsizei x)
{
    return (x & (x-1)) == 0;
}

/* Each WebGL object class WebGLFoo wants to:
 *  - inherit WebGLRefCountedObject<WebGLFoo>
 *  - implement a Delete() method
 *  - have its destructor call DeleteOnce()
 * 
 * This base class provides two features to WebGL object types:
 * 1. support for OpenGL object reference counting
 * 2. support for OpenGL deletion statuses
 *
 ***** 1. OpenGL object reference counting *****
 *
 * WebGL objects such as WebGLTexture's really have two different refcounts:
 * the XPCOM refcount, that is directly exposed to JavaScript, and the OpenGL
 * refcount.
 *
 * For example, when in JavaScript one does: var newname = existingTexture;
 * that increments the XPCOM refcount, but doesn't affect the OpenGL refcount.
 * When one attaches the texture to a framebuffer object, that does increment
 * its OpenGL refcount (and also its XPCOM refcount, to prevent the regular
 * XPCOM refcounting mechanism from destroying objects prematurely).
 *
 * The actual OpenGL refcount is opaque to us (it's internal to the OpenGL
 * implementation) but is affects the WebGL semantics that we have to implement:
 * for example, a WebGLTexture that is attached to a WebGLFramebuffer must not
 * be actually deleted, even if deleteTexture has been called on it, and even
 * if JavaScript doesn't have references to it anymore. We can't just rely on
 * OpenGL to keep alive the underlying OpenGL texture for us, for a variety of
 * reasons, most importantly: we'd need to know when OpenGL objects are actually
 * deleted, and OpenGL doesn't notify us about that, so we would have to query
 * status very often with glIsXxx calls which isn't practical.
 *
 * This means that we have to keep track of the OpenGL refcount ourselves,
 * in addition to the XPCOM refcount.
 *
 * This class implements such a refcount, see the mWebGLRefCnt
 * member. In order to avoid name clashes (with regular XPCOM refcounting)
 * in the derived class, we prefix members with 'WebGL', whence the names
 * WebGLAddRef, WebGLRelease, etc.
 *
 * In practice, WebGLAddRef and WebGLRelease are only called from the
 * WebGLRefPtr class.
 *
 ***** 2. OpenGL deletion statuses *****
 *
 * In OpenGL, an object can go through 3 different deletion statuses during its
 * lifetime, which correspond to the 3 enum values for DeletionStatus in this class:
 *  - the Default status, which it has from its creation to when the
 *    suitable glDeleteXxx function is called on it;
 *  - the DeleteRequested status, which is has from when the suitable glDeleteXxx
 *    function is called on it to when it is no longer referenced by other OpenGL
 *    objects. For example, a texture that is attached to a non-current FBO
 *    will enter that status when glDeleteTexture is called on it. For objects
 *    with that status, GL_DELETE_STATUS queries return true, but glIsXxx
 *    functions still return true.
 *  - the Deleted status, which is the status of objects on which the
 *    suitable glDeleteXxx function has been called, and that are not referenced
 *    by other OpenGL objects.
 *
 * This state is stored in the mDeletionStatus member of this class.
 *
 * When the GL refcount hits zero, if the status is DeleteRequested then we call
 * the Delete() method on the derived class and the status becomes Deleted. This is
 * what the MaybeDelete() function does.
 * 
 * The DeleteOnce() function implemented here is a helper to ensure that we don't
 * call Delete() twice on the same object. Since the derived class' destructor
 * needs to call DeleteOnce() which calls Delete(), we can't allow either to be
 * virtual. Strictly speaking, we could let them be virtual if the derived class
 * were final, but that would be impossible to enforce and would lead to strange
 * bugs if it were subclassed.
 *
 * This WebGLRefCountedObject class takes the Derived type
 * as template parameter, as a means to allow DeleteOnce to call Delete()
 * on the Derived class, without either method being virtual. This is a common
 * C++ pattern known as the "curiously recursive template pattern (CRTP)".
 */
template<typename Derived>
class WebGLRefCountedObject
{
public:
    enum DeletionStatus { Default, DeleteRequested, Deleted };

    WebGLRefCountedObject()
      : mDeletionStatus(Default)
    { }

    ~WebGLRefCountedObject() {
        NS_ABORT_IF_FALSE(mWebGLRefCnt == 0, "destroying WebGL object still referenced by other WebGL objects");
        NS_ABORT_IF_FALSE(mDeletionStatus == Deleted, "Derived class destructor must call DeleteOnce()");
    }

    // called by WebGLRefPtr
    void WebGLAddRef() {
        ++mWebGLRefCnt;
    }

    // called by WebGLRefPtr
    void WebGLRelease() {
        NS_ABORT_IF_FALSE(mWebGLRefCnt > 0, "releasing WebGL object with WebGL refcnt already zero");
        --mWebGLRefCnt;
        MaybeDelete();
    }

    // this is the function that WebGL.deleteXxx() functions want to call
    void RequestDelete() {
        if (mDeletionStatus == Default)
            mDeletionStatus = DeleteRequested;
        MaybeDelete();
    }

    bool IsDeleted() const {
        return mDeletionStatus == Deleted;
    }

    bool IsDeleteRequested() const {
        return mDeletionStatus != Default;
    }

    void DeleteOnce() {
        if (mDeletionStatus != Deleted) {
            static_cast<Derived*>(this)->Delete();
            mDeletionStatus = Deleted;
        }
    }

private:
    void MaybeDelete() {
        if (mWebGLRefCnt == 0 &&
            mDeletionStatus == DeleteRequested)
        {
            DeleteOnce();
        }
    }

protected:
    nsAutoRefCnt mWebGLRefCnt;
    DeletionStatus mDeletionStatus;
};

/* This WebGLRefPtr class is meant to be used for references between WebGL objects.
 * For example, a WebGLProgram holds WebGLRefPtr's to the WebGLShader's attached
 * to it.
 *
 * Why the need for a separate refptr class? The only special thing that WebGLRefPtr
 * does is that it increments and decrements the WebGL refcount of
 * WebGLRefCountedObject's, in addition to incrementing and decrementing the
 * usual XPCOM refcount.
 *
 * This means that by using a WebGLRefPtr instead of a nsRefPtr, you ensure that
 * the WebGL refcount is incremented, which means that the object will be kept
 * alive by this reference even if the matching webgl.deleteXxx() function is
 * called on it.
 */
template<typename T>
class WebGLRefPtr
{
public:
    WebGLRefPtr()
        : mRawPtr(0)
    { }

    WebGLRefPtr(const WebGLRefPtr<T>& aSmartPtr)
        : mRawPtr(aSmartPtr.mRawPtr)
    {
        AddRefOnPtr(mRawPtr);
    }

    WebGLRefPtr(T *aRawPtr)
        : mRawPtr(aRawPtr)
    {
        AddRefOnPtr(mRawPtr);
    }

    ~WebGLRefPtr() {
        ReleasePtr(mRawPtr);
    }

    WebGLRefPtr<T>&
    operator=(const WebGLRefPtr<T>& rhs)
    {
        assign_with_AddRef(rhs.mRawPtr);
        return *this;
    }

    WebGLRefPtr<T>&
    operator=(T* rhs)
    {
        assign_with_AddRef(rhs);
        return *this;
    }

    T* get() const {
        return static_cast<T*>(mRawPtr);
    }

    operator T*() const {
        return get();
    }

    T* operator->() const {
        NS_ABORT_IF_FALSE(mRawPtr != 0, "You can't dereference a NULL WebGLRefPtr with operator->()!");
        return get();
    }

    T& operator*() const {
        NS_ABORT_IF_FALSE(mRawPtr != 0, "You can't dereference a NULL WebGLRefPtr with operator*()!");
        return *get();
    }

private:

    static void AddRefOnPtr(T* rawPtr) {
        if (rawPtr) {
            rawPtr->WebGLAddRef();
            rawPtr->AddRef();
        }
    }

    static void ReleasePtr(T* rawPtr) {
        if (rawPtr) {
            rawPtr->WebGLRelease(); // must be done first before Release(), as Release() might actually destroy the object
            rawPtr->Release();
        }
    }

    void assign_with_AddRef(T* rawPtr) {
        AddRefOnPtr(rawPtr);
        assign_assuming_AddRef(rawPtr);
    }

    void assign_assuming_AddRef(T* newPtr) {
        T* oldPtr = mRawPtr;
        mRawPtr = newPtr;
        ReleasePtr(oldPtr);
    }

protected:
    T *mRawPtr;
};

typedef PRUint64 WebGLMonotonicHandle;

/* WebGLFastArray offers a fast array for the use case where all what one needs is to append
 * and remove elements. Removal is fast because the array is always kept sorted with respect
 * to "monotonic handles". Appending an element returns such a "monotonic handle" which the
 * user needs to keep for future use for when it will want to remove the element.
 */
template<typename ElementType>
class WebGLFastArray
{
    struct Entry {
        ElementType mElement;
        WebGLMonotonicHandle mMonotonicHandle;

        Entry(ElementType elem, WebGLMonotonicHandle monotonicHandle)
            : mElement(elem), mMonotonicHandle(monotonicHandle)
        {}

        struct Comparator {
            bool Equals(const Entry& a, const Entry& b) const {
                return a.mMonotonicHandle == b.mMonotonicHandle;
            }
            bool LessThan(const Entry& a, const Entry& b) const {
                return a.mMonotonicHandle < b.mMonotonicHandle;
            }
        };
    };

public:
    WebGLFastArray()
        : mCurrentMonotonicHandle(0) // CheckedInt already does it, this is just defensive coding
    {}

    ElementType operator[](size_t index) const {
        return mArray[index].mElement;
    }

    size_t Length() const {
        return mArray.Length();
    }

    ElementType Last() const {
        return operator[](Length() - 1);
    }

    WebGLMonotonicHandle AppendElement(ElementType elem)
    {
        WebGLMonotonicHandle monotonicHandle = NextMonotonicHandle();
        mArray.AppendElement(Entry(elem, monotonicHandle));
        return monotonicHandle;
    }

    void RemoveElement(WebGLMonotonicHandle monotonicHandle)
    {
        mArray.RemoveElementSorted(Entry(ElementType(), monotonicHandle),
                                   typename Entry::Comparator());
    }

private:
    WebGLMonotonicHandle NextMonotonicHandle() {
        ++mCurrentMonotonicHandle;
        if (!mCurrentMonotonicHandle.valid())
            NS_RUNTIMEABORT("ran out of monotonic ids!");
        return mCurrentMonotonicHandle.value();
    }

    nsTArray<Entry> mArray;
    CheckedInt<WebGLMonotonicHandle> mCurrentMonotonicHandle;
};

// this class is a mixin for GL objects that have dimensions
// that we need to track.
class WebGLRectangleObject
{
public:
    WebGLRectangleObject()
        : mWidth(0), mHeight(0) { }

    WebGLRectangleObject(WebGLsizei width, WebGLsizei height)
        : mWidth(width), mHeight(height) { }

    WebGLsizei Width() const { return mWidth; }
    void width(WebGLsizei value) { mWidth = value; }

    WebGLsizei Height() const { return mHeight; }
    void height(WebGLsizei value) { mHeight = value; }

    void setDimensions(WebGLsizei width, WebGLsizei height) {
        mWidth = width;
        mHeight = height;
    }

    void setDimensions(WebGLRectangleObject *rect) {
        if (rect) {
            mWidth = rect->Width();
            mHeight = rect->Height();
        } else {
            mWidth = 0;
            mHeight = 0;
        }
    }

    bool HasSameDimensionsAs(const WebGLRectangleObject& other) const {
        return Width() == other.Width() && Height() == other.Height(); 
    }

protected:
    WebGLsizei mWidth;
    WebGLsizei mHeight;
};

struct WebGLContextOptions {
    // these are defaults
    WebGLContextOptions()
        : alpha(true), depth(true), stencil(false),
          premultipliedAlpha(true), antialias(true),
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
    public nsSupportsWeakReference,
    public nsITimerCallback,
    public WebGLRectangleObject
{
    friend class WebGLMemoryMultiReporterWrapper;
    friend class WebGLExtensionLoseContext;
    friend class WebGLContextUserData;

public:
    WebGLContext();
    virtual ~WebGLContext();

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(WebGLContext, nsIDOMWebGLRenderingContext)

    NS_DECL_NSIDOMWEBGLRENDERINGCONTEXT

    NS_DECL_NSITIMERCALLBACK

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

    NS_IMETHOD SetIsOpaque(bool b) { return NS_OK; };
    NS_IMETHOD SetContextOptions(nsIPropertyBag *aOptions);

    NS_IMETHOD SetIsIPC(bool b) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Redraw(const gfxRect&) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Swap(mozilla::ipc::Shmem& aBack,
                    PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h)
                    { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Swap(PRUint32 nativeID,
                    PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h)
                    { return NS_ERROR_NOT_IMPLEMENTED; }

    bool LoseContext();
    bool RestoreContext();

    nsresult SynthesizeGLError(WebGLenum err);
    nsresult SynthesizeGLError(WebGLenum err, const char *fmt, ...);

    nsresult ErrorInvalidEnum(const char *fmt = 0, ...);
    nsresult ErrorInvalidOperation(const char *fmt = 0, ...);
    nsresult ErrorInvalidValue(const char *fmt = 0, ...);
    nsresult ErrorInvalidFramebufferOperation(const char *fmt = 0, ...);
    nsresult ErrorInvalidEnumInfo(const char *info, PRUint32 enumvalue) {
        return ErrorInvalidEnum("%s: invalid enum value 0x%x", info, enumvalue);
    }
    nsresult ErrorOutOfMemory(const char *fmt = 0, ...);

    const char *ErrorName(GLenum error);

    nsresult DummyFramebufferOperation(const char *info);

    WebGLTexture *activeBoundTextureForTarget(WebGLenum target) {
        return target == LOCAL_GL_TEXTURE_2D ? mBound2DTextures[mActiveTexture]
                                             : mBoundCubeMapTextures[mActiveTexture];
    }

    already_AddRefed<CanvasLayer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                                 CanvasLayer *aOldLayer,
                                                 LayerManager *aManager);
    void MarkContextClean() { mInvalidated = false; }

    // a number that increments every time we have an event that causes
    // all context resources to be lost.
    PRUint32 Generation() { return mGeneration.value(); }

    const WebGLRectangleObject *FramebufferRectangleObject() const;

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
    
    bool MinCapabilityMode() const {
        return mMinCapability;
    }

    // See the comment over WebGLContext::Notify() for more information on this.
    bool ShouldEnableRobustnessTimer() {
        return mHasRobustness ||
               IsExtensionEnabled(WebGL_MOZ_WEBGL_lose_context) ||
               (gl != nsnull && gl->GetContextType() == gl::GLContext::ContextTypeEGL);
    }

    // Sets up the GL_ARB_robustness timer if it isn't already, so that if the
    // driver gets restarted, the context may get reset with it.
    void SetupRobustnessTimer() {
        if (!ShouldEnableRobustnessTimer())
            return;

        // If the timer was already running, don't restart it here. Instead,
        // wait until the previous call is done, then fire it one more time.
        // This is an optimization to prevent unnecessary cross-communication
        // between threads.
        if (mRobustnessTimerRunning) {
            mDrawSinceRobustnessTimerSet = true;
            return;
        }
        
        mContextRestorer->InitWithCallback(static_cast<nsITimerCallback*>(this),
                                           PR_MillisecondsToInterval(1000),
                                           nsITimer::TYPE_ONE_SHOT);
        mRobustnessTimerRunning = true;
        mDrawSinceRobustnessTimerSet = false;
    }

    void TerminateRobustnessTimer() {
        if (mRobustnessTimerRunning) {
            mContextRestorer->Cancel();
            mRobustnessTimerRunning = false;
        }
    }

protected:
    void SetDontKnowIfNeedFakeBlack() {
        mFakeBlackStatus = DontKnowIfNeedFakeBlack;
    }

    bool NeedFakeBlack();
    void BindFakeBlackTextures();
    void UnbindFakeBlackTextures();

    int WhatDoesVertexAttrib0Need();
    bool DoFakeVertexAttrib0(WebGLuint vertexCount);
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

    CheckedUint32 mGeneration;

    WebGLContextOptions mOptions;

    bool mInvalidated;
    bool mResetLayer;
    bool mVerbose;
    bool mOptionsFrozen;
    bool mMinCapability;
    bool mDisableExtensions;
    bool mHasRobustness;

    template<typename WebGLObjectType>
    void DeleteWebGLObjectsArray(nsTArray<WebGLObjectType>& array);

    WebGLuint mActiveTexture;
    WebGLenum mWebGLError;

    // whether shader validation is supported
    bool mShaderValidation;

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

    // Represents current status, or state, of the context. That is, is it lost
    // or stable and what part of the context lost process are we currently at.
    // This is used to support the WebGL spec's asyncronous nature in handling
    // context loss.
    enum ContextStatus {
        // The context is stable; there either are none or we don't know of any.
        ContextStable,
        // The context has been lost, but we have not yet sent an event to the
        // script informing it of this.
        ContextLostAwaitingEvent,
        // The context has been lost, and we have sent the script an event
        // informing it of this.
        ContextLost,
        // The context is lost, an event has been sent to the script, and the
        // script correctly handled the event. We are waiting for the context to
        // be restored.
        ContextLostAwaitingRestore
    };

    // extensions
    enum WebGLExtensionID {
        WebGL_OES_texture_float,
        WebGL_OES_standard_derivatives,
        WebGL_EXT_texture_filter_anisotropic,
        WebGL_MOZ_WEBGL_lose_context,
        WebGLExtensionID_Max
    };
    nsCOMPtr<WebGLExtension> mEnabledExtensions[WebGLExtensionID_Max];
    bool IsExtensionEnabled(WebGLExtensionID ext) const {
        NS_ABORT_IF_FALSE(ext >= 0 && ext < WebGLExtensionID_Max, "bogus index!");
        return mEnabledExtensions[ext] != nsnull;
    }
    bool IsExtensionSupported(WebGLExtensionID ei);

    bool InitAndValidateGL();
    bool ValidateBuffers(PRInt32* maxAllowedCount, const char *info);
    bool ValidateCapabilityEnum(WebGLenum cap, const char *info);
    bool ValidateBlendEquationEnum(WebGLenum cap, const char *info);
    bool ValidateBlendFuncDstEnum(WebGLenum mode, const char *info);
    bool ValidateBlendFuncSrcEnum(WebGLenum mode, const char *info);
    bool ValidateBlendFuncEnumsCompatibility(WebGLenum sfactor, WebGLenum dfactor, const char *info);
    bool ValidateTextureTargetEnum(WebGLenum target, const char *info);
    bool ValidateComparisonEnum(WebGLenum target, const char *info);
    bool ValidateStencilOpEnum(WebGLenum action, const char *info);
    bool ValidateFaceEnum(WebGLenum face, const char *info);
    bool ValidateBufferUsageEnum(WebGLenum target, const char *info);
    bool ValidateTexFormatAndType(WebGLenum format, WebGLenum type, int jsArrayType,
                                      PRUint32 *texelSize, const char *info);
    bool ValidateDrawModeEnum(WebGLenum mode, const char *info);
    bool ValidateAttribIndex(WebGLuint index, const char *info);
    bool ValidateStencilParamsForDrawCall();
    
    bool ValidateGLSLVariableName(const nsAString& name, const char *info);
    bool ValidateGLSLCharacter(PRUnichar c);
    bool ValidateGLSLString(const nsAString& string, const char *info);

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
                             int srcFormat, bool srcPremultiplied);
    nsresult TexSubImage2D_base(WebGLenum target, WebGLint level,
                                WebGLint xoffset, WebGLint yoffset,
                                WebGLsizei width, WebGLsizei height, WebGLsizei srcStrideOrZero,
                                WebGLenum format, WebGLenum type,
                                void *pixels, PRUint32 byteLength,
                                int jsArrayType,
                                int srcFormat, bool srcPremultiplied);
    nsresult ReadPixels_base(WebGLint x, WebGLint y, WebGLsizei width, WebGLsizei height,
                             WebGLenum format, WebGLenum type, JSObject* pixels);
    nsresult TexParameter_base(WebGLenum target, WebGLenum pname,
                               WebGLint *intParamPtr, WebGLfloat *floatParamPtr);

    void ConvertImage(size_t width, size_t height, size_t srcStride, size_t dstStride,
                      const PRUint8*src, PRUint8 *dst,
                      int srcFormat, bool srcPremultiplied,
                      int dstFormat, bool dstPremultiplied,
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
    bool GetConcreteObject(const char *info,
                             BaseInterfaceType *aInterface,
                             ConcreteObjectType **aConcreteObject,
                             bool *isNull = 0,
                             bool *isDeleted = 0,
                             bool generateErrors = true);

    template<class ConcreteObjectType, class BaseInterfaceType>
    bool GetConcreteObjectAndGLName(const char *info,
                                      BaseInterfaceType *aInterface,
                                      ConcreteObjectType **aConcreteObject,
                                      WebGLuint *aGLObjectName,
                                      bool *isNull = 0,
                                      bool *isDeleted = 0);

    template<class ConcreteObjectType, class BaseInterfaceType>
    bool GetGLName(const char *info,
                     BaseInterfaceType *aInterface,
                     WebGLuint *aGLObjectName,
                     bool *isNull = 0,
                     bool *isDeleted = 0);

    template<class ConcreteObjectType, class BaseInterfaceType>
    bool CanGetConcreteObject(const char *info,
                                BaseInterfaceType *aInterface,
                                bool *isNull = 0,
                                bool *isDeleted = 0);

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

    void MaybeRestoreContext();
    bool IsContextStable();
    void ForceLoseContext();
    void ForceRestoreContext();

    // the buffers bound to the current program's attribs
    nsTArray<WebGLVertexAttribData> mAttribBuffers;

    nsTArray<WebGLRefPtr<WebGLTexture> > mBound2DTextures;
    nsTArray<WebGLRefPtr<WebGLTexture> > mBoundCubeMapTextures;

    WebGLRefPtr<WebGLBuffer> mBoundArrayBuffer;
    WebGLRefPtr<WebGLBuffer> mBoundElementArrayBuffer;

    WebGLRefPtr<WebGLProgram> mCurrentProgram;

    PRUint32 mMaxFramebufferColorAttachments;

    WebGLRefPtr<WebGLFramebuffer> mBoundFramebuffer;
    WebGLRefPtr<WebGLRenderbuffer> mBoundRenderbuffer;

    WebGLFastArray<WebGLTexture*> mTextures;
    WebGLFastArray<WebGLBuffer*> mBuffers;
    WebGLFastArray<WebGLProgram*> mPrograms;
    WebGLFastArray<WebGLShader*> mShaders;
    WebGLFastArray<WebGLRenderbuffer*> mRenderbuffers;
    WebGLFastArray<WebGLFramebuffer*> mFramebuffers;
    WebGLFastArray<WebGLUniformLocation*> mUniformLocations;

    // PixelStore parameters
    PRUint32 mPixelStorePackAlignment, mPixelStoreUnpackAlignment, mPixelStoreColorspaceConversion;
    bool mPixelStoreFlipY, mPixelStorePremultiplyAlpha;

    FakeBlackStatus mFakeBlackStatus;

    WebGLuint mBlackTexture2D, mBlackTextureCubeMap;
    bool mBlackTexturesAreInitialized;

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

    nsCOMPtr<nsITimer> mContextRestorer;
    bool mAllowRestore;
    bool mRobustnessTimerRunning;
    bool mDrawSinceRobustnessTimerSet;
    ContextStatus mContextStatus;
    bool mContextLostErrorSet;
    bool mContextLostDueToTest;

#ifdef XP_MACOSX
    // see bug 713305. This RAII helper guarantees that we're on the discrete GPU, during its lifetime
    // Debouncing note: we don't want to switch GPUs too frequently, so try to not create and destroy
    // these objects at high frequency. Having WebGLContext's hold one such object seems fine,
    // because WebGLContext objects only go away during GC, which shouldn't happen too frequently.
    // If in the future GC becomes much more frequent, we may have to revisit then (maybe use a timer).
    ForceDiscreteGPUHelperCGL mForceDiscreteGPUHelper;
#endif


public:
    // console logging helpers
    static void LogMessage(const char *fmt, ...);
    static void LogMessage(const char *fmt, va_list ap);
    void LogMessageIfVerbose(const char *fmt, ...);
    void LogMessageIfVerbose(const char *fmt, va_list ap);

    friend class WebGLTexture;
    friend class WebGLFramebuffer;
    friend class WebGLRenderbuffer;
    friend class WebGLProgram;
    friend class WebGLBuffer;
    friend class WebGLShader;
    friend class WebGLUniformLocation;
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

    bool IsCompatibleWithContext(WebGLContext *other) {
        return mContext == other &&
            mContextGeneration == other->Generation();
    }

protected:
    WebGLContext *mContext;
    PRUint32 mContextGeneration;
};

struct WebGLVertexAttribData {
    // note that these initial values are what GL initializes vertex attribs to
    WebGLVertexAttribData()
        : buf(0), stride(0), size(4), byteOffset(0),
          type(LOCAL_GL_FLOAT), enabled(false), normalized(false)
    { }

    WebGLRefPtr<WebGLBuffer> buf;
    WebGLuint stride;
    WebGLuint size;
    GLuint byteOffset;
    GLenum type;
    bool enabled;
    bool normalized;

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

class WebGLBuffer MOZ_FINAL
    : public nsIWebGLBuffer
    , public WebGLRefCountedObject<WebGLBuffer>
    , public WebGLContextBoundObject
{
public:
    WebGLBuffer(WebGLContext *context)
        : WebGLContextBoundObject(context)
        , mHasEverBeenBound(false)
        , mByteLength(0)
        , mTarget(LOCAL_GL_NONE)
        , mData(nsnull)
    {
        mContext->MakeContextCurrent();
        mContext->gl->fGenBuffers(1, &mGLName);
        mMonotonicHandle = mContext->mBuffers.AppendElement(this);
    }

    ~WebGLBuffer() {
        DeleteOnce();
    }

    void Delete() {
        mContext->MakeContextCurrent();
        mContext->gl->fDeleteBuffers(1, &mGLName);
        free(mData);
        mData = nsnull;
        mByteLength = 0;
        mContext->mBuffers.RemoveElement(mMonotonicHandle);
    }

    size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const {
        return aMallocSizeOf(this) + aMallocSizeOf(mData);
    }
   
    bool HasEverBeenBound() { return mHasEverBeenBound; }
    void SetHasEverBeenBound(bool x) { mHasEverBeenBound = x; }
    GLuint GLName() const { return mGLName; }
    GLuint ByteLength() const { return mByteLength; }
    GLenum Target() const { return mTarget; }
    const void *Data() const { return mData; }

    void SetByteLength(GLuint byteLength) { mByteLength = byteLength; }
    void SetTarget(GLenum target) { mTarget = target; }

    // element array buffers are the only buffers for which we need to keep a copy of the data.
    // this method assumes that the byte length has previously been set by calling SetByteLength.
    bool CopyDataIfElementArray(const void* data) {
        if (mTarget == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
            mData = realloc(mData, mByteLength);
            if (!mData) {
                mByteLength = 0;
                return false;
            }
            memcpy(mData, data, mByteLength);
        }
        return true;
    }

    // same comments as for CopyElementArrayData
    bool ZeroDataIfElementArray() {
        if (mTarget == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
            mData = realloc(mData, mByteLength);
            if (!mData) {
                mByteLength = 0;
                return false;
            }
            memset(mData, 0, mByteLength);
        }
        return true;
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
      mHasCachedMaxUbyteElement = false;
      mHasCachedMaxUshortElement = false;
    }

    PRInt32 FindMaxUbyteElement() {
      if (mHasCachedMaxUbyteElement) {
        return mCachedMaxUbyteElement;
      } else {
        mHasCachedMaxUbyteElement = true;
        mCachedMaxUbyteElement = FindMaxElementInSubArray<GLubyte>(mByteLength, 0);
        return mCachedMaxUbyteElement;
      }
    }

    PRInt32 FindMaxUshortElement() {
      if (mHasCachedMaxUshortElement) {
        return mCachedMaxUshortElement;
      } else {
        mHasCachedMaxUshortElement = true;
        mCachedMaxUshortElement = FindMaxElementInSubArray<GLshort>(mByteLength>>1, 0);
        return mCachedMaxUshortElement;
      }
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLBUFFER

protected:

    WebGLuint mGLName;
    bool mHasEverBeenBound;
    GLuint mByteLength;
    GLenum mTarget;
    WebGLMonotonicHandle mMonotonicHandle;

    PRUint8 mCachedMaxUbyteElement;
    bool mHasCachedMaxUbyteElement;
    PRUint16 mCachedMaxUshortElement;
    bool mHasCachedMaxUshortElement;

    void* mData; // in the case of an Element Array Buffer, we keep a copy.
};

class WebGLTexture MOZ_FINAL
    : public nsIWebGLTexture
    , public WebGLRefCountedObject<WebGLTexture>
    , public WebGLContextBoundObject
{
public:
    WebGLTexture(WebGLContext *context)
        : WebGLContextBoundObject(context)
        , mHasEverBeenBound(false)
        , mTarget(0)
        , mMinFilter(LOCAL_GL_NEAREST_MIPMAP_LINEAR)
        , mMagFilter(LOCAL_GL_LINEAR)
        , mWrapS(LOCAL_GL_REPEAT)
        , mWrapT(LOCAL_GL_REPEAT)
        , mFacesCount(0)
        , mMaxLevelWithCustomImages(0)
        , mHaveGeneratedMipmap(false)
        , mFakeBlackStatus(DoNotNeedFakeBlack)
    {
        mContext->MakeContextCurrent();
        mContext->gl->fGenTextures(1, &mGLName);
        mMonotonicHandle = mContext->mTextures.AppendElement(this);
    }

    ~WebGLTexture() {
        DeleteOnce();
    }

    void Delete() {
        mImageInfos.Clear();
        mContext->MakeContextCurrent();
        mContext->gl->fDeleteTextures(1, &mGLName);
        mContext->mTextures.RemoveElement(mMonotonicHandle);
    }

    bool HasEverBeenBound() { return mHasEverBeenBound; }
    void SetHasEverBeenBound(bool x) { mHasEverBeenBound = x; }
    WebGLuint GLName() { return mGLName; }
    GLenum Target() const { return mTarget; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLTEXTURE

protected:

    friend class WebGLContext;
    friend class WebGLFramebuffer;

    bool mHasEverBeenBound;
    WebGLuint mGLName;

    // we store information about the various images that are part of
    // this texture (cubemap faces, mipmap levels)

public:

    class ImageInfo : public WebGLRectangleObject {
    public:
        ImageInfo() : mFormat(0), mType(0), mIsDefined(false) {}
        ImageInfo(WebGLsizei width, WebGLsizei height,
                  WebGLenum format, WebGLenum type)
            : WebGLRectangleObject(width, height), mFormat(format), mType(type), mIsDefined(true) {}

        bool operator==(const ImageInfo& a) const {
            return mWidth == a.mWidth && mHeight == a.mHeight &&
                   mFormat == a.mFormat && mType == a.mType;
        }
        bool operator!=(const ImageInfo& a) const {
            return !(*this == a);
        }
        bool IsSquare() const {
            return mWidth == mHeight;
        }
        bool IsPositive() const {
            return mWidth > 0 && mHeight > 0;
        }
        bool IsPowerOfTwo() const {
            return is_pot_assuming_nonnegative(mWidth) &&
                   is_pot_assuming_nonnegative(mHeight); // negative sizes should never happen (caught in texImage2D...)
        }
        PRInt64 MemoryUsage() const {
            if (!mIsDefined)
                return 0;
            PRInt64 texelSize = WebGLContext::GetTexelSize(mFormat, mType);
            return PRInt64(mWidth) * PRInt64(mHeight) * texelSize;
        }
        WebGLenum Format() const { return mFormat; }
        WebGLenum Type() const { return mType; }
    protected:
        WebGLenum mFormat, mType;
        bool mIsDefined;

        friend class WebGLTexture;
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

    bool HasImageInfoAt(size_t level, size_t face) const {
        CheckedUint32 checked_index = CheckedUint32(level) * mFacesCount + face;
        return checked_index.valid() &&
               checked_index.value() < mImageInfos.Length() &&
               ImageInfoAt(level, face).mIsDefined;
    }

    static size_t FaceForTarget(WebGLenum target) {
        return target == LOCAL_GL_TEXTURE_2D ? 0 : target - LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }

    PRInt64 MemoryUsage() const {
        if (IsDeleted())
            return 0;
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

    bool mHaveGeneratedMipmap;
    FakeBlackStatus mFakeBlackStatus;

    WebGLMonotonicHandle mMonotonicHandle;

    void EnsureMaxLevelWithCustomImagesAtLeast(size_t aMaxLevelWithCustomImages) {
        mMaxLevelWithCustomImages = NS_MAX(mMaxLevelWithCustomImages, aMaxLevelWithCustomImages);
        mImageInfos.EnsureLengthAtLeast((mMaxLevelWithCustomImages + 1) * mFacesCount);
    }

    bool CheckFloatTextureFilterParams() const {
        // Without OES_texture_float_linear, only NEAREST and NEAREST_MIMPAMP_NEAREST are supported
        return (mMagFilter == LOCAL_GL_NEAREST) &&
            (mMinFilter == LOCAL_GL_NEAREST || mMinFilter == LOCAL_GL_NEAREST_MIPMAP_NEAREST);
    }

    bool AreBothWrapModesClampToEdge() const {
        return mWrapS == LOCAL_GL_CLAMP_TO_EDGE && mWrapT == LOCAL_GL_CLAMP_TO_EDGE;
    }

    bool DoesTexture2DMipmapHaveAllLevelsConsistentlyDefined(size_t face) const {
        if (mHaveGeneratedMipmap)
            return true;

        ImageInfo expected = ImageInfoAt(0, face);

        // checks if custom level>0 images are all defined up to the highest level defined
        // and have the expected dimensions
        for (size_t level = 0; level <= mMaxLevelWithCustomImages; ++level) {
            const ImageInfo& actual = ImageInfoAt(level, face);
            if (actual != expected)
                return false;
            expected.mWidth = NS_MAX(1, expected.mWidth >> 1);
            expected.mHeight = NS_MAX(1, expected.mHeight >> 1);

            // if the current level has size 1x1, we can stop here: the spec doesn't seem to forbid the existence
            // of extra useless levels.
            if (actual.mWidth == 1 && actual.mHeight == 1)
                return true;
        }

        // if we're here, we've exhausted all levels without finding a 1x1 image
        return false;
    }

public:

    void SetDontKnowIfNeedFakeBlack() {
        mFakeBlackStatus = DontKnowIfNeedFakeBlack;
        mContext->SetDontKnowIfNeedFakeBlack();
    }

    void Bind(WebGLenum aTarget) {
        // this function should only be called by bindTexture().
        // it assumes that the GL context is already current.

        bool firstTimeThisTextureIsBound = !mHasEverBeenBound;

        if (!firstTimeThisTextureIsBound && aTarget != mTarget) {
            mContext->ErrorInvalidOperation("bindTexture: this texture has already been bound to a different target");
            // very important to return here before modifying texture state! This was the place when I lost a whole day figuring
            // very strange 'invalid write' crashes.
            return;
        }

        mTarget = aTarget;

        mContext->gl->fBindTexture(mTarget, mGLName);

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

        mHasEverBeenBound = true;
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
    WebGLenum MinFilter() const { return mMinFilter; }

    bool DoesMinFilterRequireMipmap() const {
        return !(mMinFilter == LOCAL_GL_NEAREST || mMinFilter == LOCAL_GL_LINEAR);
    }

    void SetGeneratedMipmap() {
        if (!mHaveGeneratedMipmap) {
            mHaveGeneratedMipmap = true;
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
        mHaveGeneratedMipmap = false;
    }

    bool IsFirstImagePowerOfTwo() const {
        return ImageInfoAt(0, 0).IsPowerOfTwo();
    }

    bool AreAllLevel0ImageInfosEqual() const {
        for (size_t face = 1; face < mFacesCount; ++face) {
            if (ImageInfoAt(0, face) != ImageInfoAt(0, 0))
                return false;
        }
        return true;
    }

    bool IsMipmapTexture2DComplete() const {
        if (mTarget != LOCAL_GL_TEXTURE_2D)
            return false;
        if (!ImageInfoAt(0, 0).IsPositive())
            return false;
        if (mHaveGeneratedMipmap)
            return true;
        return DoesTexture2DMipmapHaveAllLevelsConsistentlyDefined(0);
    }

    bool IsCubeComplete() const {
        if (mTarget != LOCAL_GL_TEXTURE_CUBE_MAP)
            return false;
        const ImageInfo &first = ImageInfoAt(0, 0);
        if (!first.IsPositive() || !first.IsSquare())
            return false;
        return AreAllLevel0ImageInfosEqual();
    }

    bool IsMipmapCubeComplete() const {
        if (!IsCubeComplete()) // in particular, this checks that this is a cube map
            return false;
        for (size_t face = 0; face < mFacesCount; ++face) {
            if (!DoesTexture2DMipmapHaveAllLevelsConsistentlyDefined(face))
                return false;
        }
        return true;
    }

    bool NeedFakeBlack() {
        // handle this case first, it's the generic case
        if (mFakeBlackStatus == DoNotNeedFakeBlack)
            return false;

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
                    return true;
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
                bool areAllLevel0ImagesPOT = true;
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

class WebGLShader MOZ_FINAL
    : public nsIWebGLShader
    , public WebGLRefCountedObject<WebGLShader>
    , public WebGLContextBoundObject
{
public:
    WebGLShader(WebGLContext *context, WebGLenum stype)
        : WebGLContextBoundObject(context)
        , mType(stype)
        , mNeedsTranslation(true)
    {
        mContext->MakeContextCurrent();
        mGLName = mContext->gl->fCreateShader(mType);
        mMonotonicHandle = mContext->mShaders.AppendElement(this);
    }

    ~WebGLShader() {
        DeleteOnce();
    }
    
    size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) {
        return aMallocSizeOf(this) +
               mSource.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
               mTranslationLog.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }

    void Delete() {
        mSource.Truncate();
        mTranslationLog.Truncate();
        mContext->MakeContextCurrent();
        mContext->gl->fDeleteShader(mGLName);
        mContext->mShaders.RemoveElement(mMonotonicHandle);
    }

    WebGLuint GLName() { return mGLName; }
    WebGLenum ShaderType() { return mType; }

    void SetSource(const nsAString& src) {
        // XXX do some quick gzip here maybe -- getting this will be very rare
        mSource.Assign(src);
    }

    const nsString& Source() const { return mSource; }

    void SetNeedsTranslation() { mNeedsTranslation = true; }
    bool NeedsTranslation() const { return mNeedsTranslation; }

    void SetTranslationSuccess() {
        mTranslationLog.SetIsVoid(true);
        mNeedsTranslation = false;
    }

    void SetTranslationFailure(const nsCString& msg) {
        mTranslationLog.Assign(msg); 
    }

    const nsCString& TranslationLog() const { return mTranslationLog; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLSHADER

protected:

    WebGLuint mGLName;
    WebGLenum mType;
    nsString mSource;
    nsCString mTranslationLog;
    bool mNeedsTranslation;
    WebGLMonotonicHandle mMonotonicHandle;
};

class WebGLProgram MOZ_FINAL
    : public nsIWebGLProgram
    , public WebGLRefCountedObject<WebGLProgram>
    , public WebGLContextBoundObject
{
public:
    WebGLProgram(WebGLContext *context)
        : WebGLContextBoundObject(context)
        , mLinkStatus(false)
        , mGeneration(0)
        , mUniformMaxNameLength(0)
        , mAttribMaxNameLength(0)
        , mUniformCount(0)
        , mAttribCount(0)
    {
        mContext->MakeContextCurrent();
        mGLName = mContext->gl->fCreateProgram();
        mMonotonicHandle = mContext->mPrograms.AppendElement(this);
    }

    ~WebGLProgram() {
        DeleteOnce();
    }

    void Delete() {
        DetachShaders();
        mContext->MakeContextCurrent();
        mContext->gl->fDeleteProgram(mGLName);
        mContext->mPrograms.RemoveElement(mMonotonicHandle);
    }

    void DetachShaders() {
        mAttachedShaders.Clear();
    }

    WebGLuint GLName() { return mGLName; }
    const nsTArray<WebGLRefPtr<WebGLShader> >& AttachedShaders() const { return mAttachedShaders; }
    bool LinkStatus() { return mLinkStatus; }
    PRUint32 Generation() const { return mGeneration.value(); }
    void SetLinkStatus(bool val) { mLinkStatus = val; }

    bool ContainsShader(WebGLShader *shader) {
        return mAttachedShaders.Contains(shader);
    }

    // return true if the shader wasn't already attached
    bool AttachShader(WebGLShader *shader) {
        if (ContainsShader(shader))
            return false;
        mAttachedShaders.AppendElement(shader);

        mContext->MakeContextCurrent();
        mContext->gl->fAttachShader(GLName(), shader->GLName());

        return true;
    }

    // return true if the shader was found and removed
    bool DetachShader(WebGLShader *shader) {
        if (!mAttachedShaders.RemoveElement(shader))
            return false;

        mContext->MakeContextCurrent();
        mContext->gl->fDetachShader(GLName(), shader->GLName());

        return true;
    }

    bool HasAttachedShaderOfType(GLenum shaderType) {
        for (PRUint32 i = 0; i < mAttachedShaders.Length(); ++i) {
            if (mAttachedShaders[i] && mAttachedShaders[i]->ShaderType() == shaderType) {
                return true;
            }
        }
        return false;
    }

    bool HasBothShaderTypesAttached() {
        return
            HasAttachedShaderOfType(LOCAL_GL_VERTEX_SHADER) &&
            HasAttachedShaderOfType(LOCAL_GL_FRAGMENT_SHADER);
    }

    bool NextGeneration()
    {
        if (!(mGeneration+1).valid())
            return false; // must exit without changing mGeneration
        ++mGeneration;
        return true;
    }

    /* Called only after LinkProgram */
    bool UpdateInfo(gl::GLContext *gl);

    /* Getters for cached program info */
    WebGLint UniformMaxNameLength() const { return mUniformMaxNameLength; }
    WebGLint AttribMaxNameLength() const { return mAttribMaxNameLength; }
    WebGLint UniformCount() const { return mUniformCount; }
    WebGLint AttribCount() const { return mAttribCount; }
    bool IsAttribInUse(unsigned i) const { return mAttribsInUse[i]; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLPROGRAM

protected:

    WebGLuint mGLName;
    bool mLinkStatus;
    // attached shaders of the program object
    nsTArray<WebGLRefPtr<WebGLShader> > mAttachedShaders;
    CheckedUint32 mGeneration;

    // post-link data

    GLint mUniformMaxNameLength;
    GLint mAttribMaxNameLength;
    GLint mUniformCount;
    GLint mAttribCount;
    std::vector<bool> mAttribsInUse;
    WebGLMonotonicHandle mMonotonicHandle;
};

class WebGLRenderbuffer MOZ_FINAL
    : public nsIWebGLRenderbuffer
    , public WebGLRefCountedObject<WebGLRenderbuffer>
    , public WebGLRectangleObject
    , public WebGLContextBoundObject
{
public:
    WebGLRenderbuffer(WebGLContext *context)
        : WebGLContextBoundObject(context)
        , mInternalFormat(0)
        , mInternalFormatForGL(0)
        , mHasEverBeenBound(false)
        , mInitialized(false)
    {

        mContext->MakeContextCurrent();
        mContext->gl->fGenRenderbuffers(1, &mGLName);
        mMonotonicHandle = mContext->mRenderbuffers.AppendElement(this);
    }

    ~WebGLRenderbuffer() {
        DeleteOnce();
    }

    void Delete() {
        mContext->MakeContextCurrent();
        mContext->gl->fDeleteRenderbuffers(1, &mGLName);
        mContext->mRenderbuffers.RemoveElement(mMonotonicHandle);
    }

    bool HasEverBeenBound() { return mHasEverBeenBound; }
    void SetHasEverBeenBound(bool x) { mHasEverBeenBound = x; }
    WebGLuint GLName() const { return mGLName; }

    bool Initialized() const { return mInitialized; }
    void SetInitialized(bool aInitialized) { mInitialized = aInitialized; }

    WebGLenum InternalFormat() const { return mInternalFormat; }
    void SetInternalFormat(WebGLenum aInternalFormat) { mInternalFormat = aInternalFormat; }
    
    WebGLenum InternalFormatForGL() const { return mInternalFormatForGL; }
    void SetInternalFormatForGL(WebGLenum aInternalFormatForGL) { mInternalFormatForGL = aInternalFormatForGL; }
    
    PRInt64 MemoryUsage() const {
        PRInt64 pixels = PRInt64(Width()) * PRInt64(Height());
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

    WebGLuint mGLName;
    WebGLenum mInternalFormat;
    WebGLenum mInternalFormatForGL;
    WebGLMonotonicHandle mMonotonicHandle;
    bool mHasEverBeenBound;
    bool mInitialized;

    friend class WebGLFramebuffer;
};

class WebGLFramebufferAttachment
{
    // deleting a texture or renderbuffer immediately detaches it
    WebGLRefPtr<WebGLTexture> mTexturePtr;
    WebGLRefPtr<WebGLRenderbuffer> mRenderbufferPtr;
    WebGLenum mAttachmentPoint;
    WebGLint mTextureLevel;
    WebGLenum mTextureCubeMapFace;

public:
    WebGLFramebufferAttachment(WebGLenum aAttachmentPoint)
        : mAttachmentPoint(aAttachmentPoint)
    {}

    bool IsDefined() const {
        return Texture() || Renderbuffer();
    }

    bool IsDeleteRequested() const {
        return Texture() ? Texture()->IsDeleteRequested()
             : Renderbuffer() ? Renderbuffer()->IsDeleteRequested()
             : false;
    }

    bool HasAlpha() const {
        WebGLenum format = 0;
        if (Texture() && Texture()->HasImageInfoAt(mTextureLevel, mTextureCubeMapFace))
            format = Texture()->ImageInfoAt(mTextureLevel, mTextureCubeMapFace).Format();
        else if (Renderbuffer())
            format = Renderbuffer()->InternalFormat();
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
    }
    void SetRenderbuffer(WebGLRenderbuffer *rb) {
        mTexturePtr = nsnull;
        mRenderbufferPtr = rb;
    }
    const WebGLTexture *Texture() const {
        return mTexturePtr;
    }
    WebGLTexture *Texture() {
        return mTexturePtr;
    }
    const WebGLRenderbuffer *Renderbuffer() const {
        return mRenderbufferPtr;
    }
    WebGLRenderbuffer *Renderbuffer() {
        return mRenderbufferPtr;
    }
    WebGLint TextureLevel() const {
        return mTextureLevel;
    }
    WebGLenum TextureCubeMapFace() const {
        return mTextureCubeMapFace;
    }

    bool HasUninitializedRenderbuffer() const {
        return mRenderbufferPtr && !mRenderbufferPtr->Initialized();
    }

    void Reset() {
        mTexturePtr = nsnull;
        mRenderbufferPtr = nsnull;
    }

    const WebGLRectangleObject* RectangleObject() const {
        if (Texture() && Texture()->HasImageInfoAt(mTextureLevel, mTextureCubeMapFace))
            return &Texture()->ImageInfoAt(mTextureLevel, mTextureCubeMapFace);
        else if (Renderbuffer())
            return Renderbuffer();
        else
            return nsnull;
    }
    bool HasSameDimensionsAs(const WebGLFramebufferAttachment& other) const {
        const WebGLRectangleObject *thisRect = RectangleObject();
        const WebGLRectangleObject *otherRect = other.RectangleObject();
        return thisRect &&
               otherRect &&
               thisRect->HasSameDimensionsAs(*otherRect);
    }

    bool IsComplete() const {
        const WebGLRectangleObject *thisRect = RectangleObject();

        if (!thisRect ||
            !thisRect->Width() ||
            !thisRect->Height())
            return false;

        if (mTexturePtr)
            return mAttachmentPoint == LOCAL_GL_COLOR_ATTACHMENT0;

        if (mRenderbufferPtr) {
            WebGLenum format = mRenderbufferPtr->InternalFormat();
            switch (mAttachmentPoint) {
                case LOCAL_GL_COLOR_ATTACHMENT0:
                    return format == LOCAL_GL_RGB565 ||
                           format == LOCAL_GL_RGB5_A1 ||
                           format == LOCAL_GL_RGBA4;
                case LOCAL_GL_DEPTH_ATTACHMENT:
                    return format == LOCAL_GL_DEPTH_COMPONENT16;
                case LOCAL_GL_STENCIL_ATTACHMENT:
                    return format == LOCAL_GL_STENCIL_INDEX8;
                case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
                    return format == LOCAL_GL_DEPTH_STENCIL;
                default:
                    NS_ABORT(); // should have been validated earlier
            }
        }

        NS_ABORT(); // should never get there
        return false;
    }
};

class WebGLFramebuffer MOZ_FINAL
    : public nsIWebGLFramebuffer
    , public WebGLRefCountedObject<WebGLFramebuffer>
    , public WebGLContextBoundObject
{
public:
    WebGLFramebuffer(WebGLContext *context)
        : WebGLContextBoundObject(context)
        , mHasEverBeenBound(false)
        , mColorAttachment(LOCAL_GL_COLOR_ATTACHMENT0)
        , mDepthAttachment(LOCAL_GL_DEPTH_ATTACHMENT)
        , mStencilAttachment(LOCAL_GL_STENCIL_ATTACHMENT)
        , mDepthStencilAttachment(LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
    {
        mContext->MakeContextCurrent();
        mContext->gl->fGenFramebuffers(1, &mGLName);
        mMonotonicHandle = mContext->mFramebuffers.AppendElement(this);
    }

    ~WebGLFramebuffer() {
        DeleteOnce();
    }

    void Delete() {
        mColorAttachment.Reset();
        mDepthAttachment.Reset();
        mStencilAttachment.Reset();
        mDepthStencilAttachment.Reset();
        mContext->MakeContextCurrent();
        mContext->gl->fDeleteFramebuffers(1, &mGLName);
        mContext->mFramebuffers.RemoveElement(mMonotonicHandle);
    }

    bool HasEverBeenBound() { return mHasEverBeenBound; }
    void SetHasEverBeenBound(bool x) { mHasEverBeenBound = x; }
    WebGLuint GLName() { return mGLName; }

    nsresult FramebufferRenderbuffer(WebGLenum target,
                                     WebGLenum attachment,
                                     WebGLenum rbtarget,
                                     nsIWebGLRenderbuffer *rbobj)
    {
        WebGLuint renderbuffername;
        bool isNull;
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
        bool isNull;
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

    bool HasIncompleteAttachment() const {
        return (mColorAttachment.IsDefined() && !mColorAttachment.IsComplete()) ||
               (mDepthAttachment.IsDefined() && !mDepthAttachment.IsComplete()) ||
               (mStencilAttachment.IsDefined() && !mStencilAttachment.IsComplete()) ||
               (mDepthStencilAttachment.IsDefined() && !mDepthStencilAttachment.IsComplete());
    }

    bool HasDepthStencilConflict() const {
        return int(mDepthAttachment.IsDefined()) +
               int(mStencilAttachment.IsDefined()) +
               int(mDepthStencilAttachment.IsDefined()) >= 2;
    }

    bool HasAttachmentsOfMismatchedDimensions() const {
        return (mDepthAttachment.IsDefined() && !mDepthAttachment.HasSameDimensionsAs(mColorAttachment)) ||
               (mStencilAttachment.IsDefined() && !mStencilAttachment.HasSameDimensionsAs(mColorAttachment)) ||
               (mDepthStencilAttachment.IsDefined() && !mDepthStencilAttachment.HasSameDimensionsAs(mColorAttachment));
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

    void DetachTexture(const WebGLTexture *tex) {
        if (mColorAttachment.Texture() == tex)
            FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0, LOCAL_GL_TEXTURE_2D, nsnull, 0);
        if (mDepthAttachment.Texture() == tex)
            FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT, LOCAL_GL_TEXTURE_2D, nsnull, 0);
        if (mStencilAttachment.Texture() == tex)
            FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT, LOCAL_GL_TEXTURE_2D, nsnull, 0);
        if (mDepthStencilAttachment.Texture() == tex)
            FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT, LOCAL_GL_TEXTURE_2D, nsnull, 0);
    }

    void DetachRenderbuffer(const WebGLRenderbuffer *rb) {
        if (mColorAttachment.Renderbuffer() == rb)
            FramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0, LOCAL_GL_RENDERBUFFER, nsnull);
        if (mDepthAttachment.Renderbuffer() == rb)
            FramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT, LOCAL_GL_RENDERBUFFER, nsnull);
        if (mStencilAttachment.Renderbuffer() == rb)
            FramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT, LOCAL_GL_RENDERBUFFER, nsnull);
        if (mDepthStencilAttachment.Renderbuffer() == rb)
            FramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT, LOCAL_GL_RENDERBUFFER, nsnull);
    }

    const WebGLRectangleObject *RectangleObject() {
        return mColorAttachment.RectangleObject();
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLFRAMEBUFFER

    bool CheckAndInitializeRenderbuffers()
    {
        // enforce WebGL section 6.5 which is WebGL-specific, hence OpenGL itself would not
        // generate the INVALID_FRAMEBUFFER_OPERATION that we need here
        if (HasDepthStencilConflict())
            return false;

        if (!mColorAttachment.HasUninitializedRenderbuffer() &&
            !mDepthAttachment.HasUninitializedRenderbuffer() &&
            !mStencilAttachment.HasUninitializedRenderbuffer() &&
            !mDepthStencilAttachment.HasUninitializedRenderbuffer())
            return true;

        // ensure INVALID_FRAMEBUFFER_OPERATION in zero-size case
        const WebGLRectangleObject *rect = mColorAttachment.RectangleObject();
        if (!rect ||
            !rect->Width() ||
            !rect->Height())
            return false;

        mContext->MakeContextCurrent();

        WebGLenum status;
        mContext->CheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER, &status);
        if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE)
            return false;

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

        mContext->ForceClearFramebufferWithDefaultValues(mask, nsIntRect(0, 0, rect->Width(), rect->Height()));

        if (mColorAttachment.HasUninitializedRenderbuffer())
            mColorAttachment.Renderbuffer()->SetInitialized(true);

        if (mDepthAttachment.HasUninitializedRenderbuffer())
            mDepthAttachment.Renderbuffer()->SetInitialized(true);

        if (mStencilAttachment.HasUninitializedRenderbuffer())
            mStencilAttachment.Renderbuffer()->SetInitialized(true);

        if (mDepthStencilAttachment.HasUninitializedRenderbuffer())
            mDepthStencilAttachment.Renderbuffer()->SetInitialized(true);

        return true;
    }

    WebGLuint mGLName;
    bool mHasEverBeenBound;

    // we only store pointers to attached renderbuffers, not to attached textures, because
    // we will only need to initialize renderbuffers. Textures are already initialized.
    WebGLFramebufferAttachment mColorAttachment,
                               mDepthAttachment,
                               mStencilAttachment,
                               mDepthStencilAttachment;

    WebGLMonotonicHandle mMonotonicHandle;
};

class WebGLUniformLocation MOZ_FINAL
    : public nsIWebGLUniformLocation
    , public WebGLContextBoundObject
    , public WebGLRefCountedObject<WebGLUniformLocation>
{
public:
    WebGLUniformLocation(WebGLContext *context, WebGLProgram *program, GLint location)
        : WebGLContextBoundObject(context)
        , mProgram(program)
        , mProgramGeneration(program->Generation())
        , mLocation(location)
    {
        mMonotonicHandle = mContext->mUniformLocations.AppendElement(this);
    }

    ~WebGLUniformLocation() {
        DeleteOnce();
    }

    void Delete() {
        mProgram = nsnull;
        mContext->mUniformLocations.RemoveElement(mMonotonicHandle);
    }

    WebGLProgram *Program() const { return mProgram; }
    GLint Location() const { return mLocation; }
    PRUint32 ProgramGeneration() const { return mProgramGeneration; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLUNIFORMLOCATION
protected:
    // nsRefPtr, not WebGLRefPtr, so that we don't prevent the program from being explicitly deleted.
    // we just want to avoid having a dangling pointer.
    nsRefPtr<WebGLProgram> mProgram;

    PRUint32 mProgramGeneration;
    GLint mLocation;
    WebGLMonotonicHandle mMonotonicHandle;
    friend class WebGLProgram;
};

class WebGLActiveInfo MOZ_FINAL
    : public nsIWebGLActiveInfo
{
public:
    WebGLActiveInfo(WebGLint size, WebGLenum type, const char *nameptr, PRUint32 namelength) :
        mSize(size),
        mType(type)
    {
        mName.AssignASCII(nameptr, namelength);
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLACTIVEINFO
protected:
    WebGLint mSize;
    WebGLenum mType;
    nsString mName;
};

class WebGLShaderPrecisionFormat MOZ_FINAL
    : public nsIWebGLShaderPrecisionFormat
{
public:
    WebGLShaderPrecisionFormat(WebGLint rangeMin, WebGLint rangeMax, WebGLint precision) :
        mRangeMin(rangeMin),
        mRangeMax(rangeMax),
        mPrecision(precision)
    {
    
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLSHADERPRECISIONFORMAT

protected:
    WebGLint mRangeMin;
    WebGLint mRangeMax;
    WebGLint mPrecision;
};

class WebGLExtension
    : public nsIWebGLExtension
    , public WebGLContextBoundObject
{
public:
    WebGLExtension(WebGLContext *baseContext)
        : WebGLContextBoundObject(baseContext)
    {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLEXTENSION
    virtual ~WebGLExtension() {}
};

inline const WebGLRectangleObject *WebGLContext::FramebufferRectangleObject() const {
    return mBoundFramebuffer ? mBoundFramebuffer->RectangleObject()
                             : static_cast<const WebGLRectangleObject*>(this);
}

/**
 ** Template implementations
 **/

/* Helper function taking a BaseInterfaceType pointer, casting it to
 * ConcreteObjectType and performing some checks along the way.
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
inline bool
WebGLContext::GetConcreteObject(const char *info,
                                BaseInterfaceType *aInterface,
                                ConcreteObjectType **aConcreteObject,
                                bool *isNull,
                                bool *isDeleted,
                                bool generateErrors)
{
    if (!aInterface) {
        if (NS_LIKELY(isNull)) {
            // non-null isNull means that the caller will accept a null arg
            *isNull = true;
            if(isDeleted) *isDeleted = false;
            *aConcreteObject = 0;
            return true;
        } else {
            if (generateErrors)
                ErrorInvalidValue("%s: null object passed as argument", info);
            return false;
        }
    }

    if (isNull)
        *isNull = false;

    // the key to why this static_cast is all we need to do (as opposed to the QueryInterface check we used to do)
    // is that since bug 638328, WebGL interfaces are marked 'builtinclass' in the IDL
    ConcreteObjectType *concrete = static_cast<ConcreteObjectType*>(aInterface);
    *aConcreteObject = concrete;

    if (!concrete->IsCompatibleWithContext(this)) {
        // the object doesn't belong to this WebGLContext
        if (generateErrors)
            ErrorInvalidOperation("%s: object from different WebGL context (or older generation of this one) "
                                  "passed as argument", info);
        return false;
    }

    if (concrete->IsDeleted()) {
        if (NS_LIKELY(isDeleted)) {
            // non-null isDeleted means that the caller will accept a deleted arg
            *isDeleted = true;
            return true;
        } else {
            if (generateErrors)
                ErrorInvalidValue("%s: deleted object passed as argument", info);
            return false;
        }
    }

    if (isDeleted)
      *isDeleted = false;

    return true;
}

/* Same as GetConcreteObject, and in addition gets the GL object name.
 * Null objects give the name 0.
 */
template<class ConcreteObjectType, class BaseInterfaceType>
inline bool
WebGLContext::GetConcreteObjectAndGLName(const char *info,
                                         BaseInterfaceType *aInterface,
                                         ConcreteObjectType **aConcreteObject,
                                         WebGLuint *aGLObjectName,
                                         bool *isNull,
                                         bool *isDeleted)
{
    bool result = GetConcreteObject(info, aInterface, aConcreteObject, isNull, isDeleted);
    if (result == false) return false;
    *aGLObjectName = *aConcreteObject ? (*aConcreteObject)->GLName() : 0;
    return true;
}

/* Same as GetConcreteObjectAndGLName when you don't need the concrete object pointer.
 */
template<class ConcreteObjectType, class BaseInterfaceType>
inline bool
WebGLContext::GetGLName(const char *info,
                        BaseInterfaceType *aInterface,
                        WebGLuint *aGLObjectName,
                        bool *isNull,
                        bool *isDeleted)
{
    ConcreteObjectType *aConcreteObject;
    return GetConcreteObjectAndGLName(info, aInterface, &aConcreteObject, aGLObjectName, isNull, isDeleted);
}

/* Same as GetConcreteObject when you only want to check if the conversion succeeds.
 */
template<class ConcreteObjectType, class BaseInterfaceType>
inline bool
WebGLContext::CanGetConcreteObject(const char *info,
                              BaseInterfaceType *aInterface,
                              bool *isNull,
                              bool *isDeleted)
{
    ConcreteObjectType *aConcreteObject;
    return GetConcreteObject(info, aInterface, &aConcreteObject, isNull, isDeleted, false);
}

class WebGLMemoryMultiReporterWrapper
{
    WebGLMemoryMultiReporterWrapper();
    ~WebGLMemoryMultiReporterWrapper();
    static WebGLMemoryMultiReporterWrapper* sUniqueInstance;

    // here we store plain pointers, not RefPtrs: we don't want the 
    // WebGLMemoryMultiReporterWrapper unique instance to keep alive all		
    // WebGLContexts ever created.
    typedef nsTArray<const WebGLContext*> ContextsArrayType;
    ContextsArrayType mContexts;
    
    nsCOMPtr<nsIMemoryMultiReporter> mReporter;

    static WebGLMemoryMultiReporterWrapper* UniqueInstance();

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

    static PRInt64 GetTextureMemoryUsed() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i)
            for (size_t j = 0; j < contexts[i]->mTextures.Length(); ++j)
              result += contexts[i]->mTextures[j]->MemoryUsage();
        return result;
    }

    static PRInt64 GetTextureCount() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i)
            result += contexts[i]->mTextures.Length();
        return result;
    }

    static PRInt64 GetBufferMemoryUsed() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i)
            for (size_t j = 0; j < contexts[i]->mBuffers.Length(); ++j)
                result += contexts[i]->mBuffers[j]->ByteLength();
        return result;
    }

    static PRInt64 GetBufferCacheMemoryUsed();

    static PRInt64 GetBufferCount() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i)
            result += contexts[i]->mBuffers.Length();
        return result;
    }

    static PRInt64 GetRenderbufferMemoryUsed() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i)
            for (size_t j = 0; j < contexts[i]->mRenderbuffers.Length(); ++j)
              result += contexts[i]->mRenderbuffers[j]->MemoryUsage();
        return result;
    }

    static PRInt64 GetRenderbufferCount() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i)
            result += contexts[i]->mRenderbuffers.Length();
        return result;
    }

    static PRInt64 GetShaderSize();

    static PRInt64 GetShaderCount() {
        const ContextsArrayType & contexts = Contexts();
        PRInt64 result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i)
            result += contexts[i]->mShaders.Length();
        return result;
    }

    static PRInt64 GetContextCount() {
        return Contexts().Length();
    }
};

}

#endif
