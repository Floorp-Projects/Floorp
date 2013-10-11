/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLOBJECTMODEL_H_
#define WEBGLOBJECTMODEL_H_

#include "nsCycleCollectionNoteChild.h"
#include "nsICanvasRenderingContextInternal.h"
#include "WebGLTypes.h"

namespace mozilla {

class WebGLBuffer;
class WebGLContext;

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
        MOZ_ASSERT(mWebGLRefCnt == 0, "destroying WebGL object still referenced by other WebGL objects");
        MOZ_ASSERT(mDeletionStatus == Deleted, "Derived class destructor must call DeleteOnce()");
    }

    // called by WebGLRefPtr
    void WebGLAddRef() {
        ++mWebGLRefCnt;
    }

    // called by WebGLRefPtr
    void WebGLRelease() {
        MOZ_ASSERT(mWebGLRefCnt > 0, "releasing WebGL object with WebGL refcnt already zero");
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
        MOZ_ASSERT(mRawPtr != 0, "You can't dereference a nullptr WebGLRefPtr with operator->()!");
        return get();
    }

    T& operator*() const {
        MOZ_ASSERT(mRawPtr != 0, "You can't dereference a nullptr WebGLRefPtr with operator*()!");
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

// This class is a mixin for objects that are tied to a specific
// context (which is to say, all of them).  They provide initialization
// as well as comparison with the current context.
class WebGLContextBoundObject
{
public:
    WebGLContextBoundObject(WebGLContext *context);

    bool IsCompatibleWithContext(WebGLContext *other);

    WebGLContext *Context() const { return mContext; }

protected:
    WebGLContext *mContext;
    uint32_t mContextGeneration;
};

// this class is a mixin for GL objects that have dimensions
// that we need to track.
class WebGLRectangleObject
{
public:
    WebGLRectangleObject()
        : mWidth(0), mHeight(0) { }

    WebGLRectangleObject(GLsizei width, GLsizei height)
        : mWidth(width), mHeight(height) { }

    GLsizei Width() const { return mWidth; }
    void width(GLsizei value) { mWidth = value; }

    GLsizei Height() const { return mHeight; }
    void height(GLsizei value) { mHeight = value; }

    void setDimensions(GLsizei width, GLsizei height) {
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
    GLsizei mWidth;
    GLsizei mHeight;
};

}// namespace mozilla

template <typename T>
inline void
ImplCycleCollectionUnlink(mozilla::WebGLRefPtr<T>& aField)
{
  aField = nullptr;
}

template <typename T>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            mozilla::WebGLRefPtr<T>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  CycleCollectionNoteChild(aCallback, aField.get(), aName, aFlags);
}

#endif
