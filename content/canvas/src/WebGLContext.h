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

#include "nsTArray.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"

#include "nsIDocShell.h"

#include "nsICanvasRenderingContextWebGL.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsWeakReference.h"
#include "nsIDOMHTMLElement.h"
#include "nsIJSNativeInitializer.h"

#include "SimpleBuffer.h"
#include "nsGLPbuffer.h"

class nsIDocShell;

namespace mozilla {

class WebGLArray;
class WebGLTexture;
class WebGLBuffer;
class WebGLProgram;
class WebGLShader;
class WebGLFramebuffer;
class WebGLRenderbuffer;

class WebGLZeroingObject;

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
    WebGLVertexAttribData()
        : buf(0), stride(0), size(0), offset(0), enabled(PR_FALSE)
    { }

    WebGLObjectRefPtr<WebGLBuffer> buf;
    GLuint stride;
    GLuint size;
    GLuint offset;
    PRBool enabled;
};

class WebGLContext :
    public nsICanvasRenderingContextWebGL,
    public nsICanvasRenderingContextInternal,
    public nsSupportsWeakReference
{
public:
    WebGLContext();
    virtual ~WebGLContext();

    NS_DECL_ISUPPORTS
    NS_DECL_NSICANVASRENDERINGCONTEXTWEBGL

    // nsICanvasRenderingContextInternal
    NS_IMETHOD SetCanvasElement(nsICanvasElement* aParentCanvas);
    NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height);
    NS_IMETHOD InitializeWithSurface(nsIDocShell *docShell, gfxASurface *surface, PRInt32 width, PRInt32 height)
        { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Render(gfxContext *ctx, gfxPattern::GraphicsFilter f);
    NS_IMETHOD GetInputStream(const char* aMimeType,
                              const PRUnichar* aEncoderOptions,
                              nsIInputStream **aStream);
    NS_IMETHOD GetThebesSurface(gfxASurface **surface);
    NS_IMETHOD SetIsOpaque(PRBool b) { return NS_OK; };

protected:
    GLES20Wrap *gl;

    nsICanvasElement* mCanvasElement;

    nsGLPbuffer *mGLPbuffer;
    PRInt32 mWidth, mHeight;

    PRBool mInvalidated;

    PRBool SafeToCreateCanvas3DContext(nsICanvasElement *canvasElement);
    PRBool ValidateGL();
    PRBool ValidateBuffers(PRUint32 count);

    void Invalidate();

    void MakeContextCurrent() { mGLPbuffer->MakeContextCurrent(); }

    nsresult TexImageElementBase(nsIDOMHTMLElement *imageOrCanvas,
                                 gfxImageSurface **imageOut);

    GLuint mActiveTexture;

    // the buffers bound to the current program's attribs
    nsTArray<WebGLVertexAttribData> mAttribBuffers;

    // the textures bound to any sampler uniforms
    nsTArray<WebGLObjectRefPtr<WebGLTexture> > mUniformTextures;

    // textures bound to 
    nsTArray<WebGLObjectRefPtr<WebGLTexture> > mBound2DTextures;
    nsTArray<WebGLObjectRefPtr<WebGLTexture> > mBoundCubeMapTextures;

    WebGLObjectRefPtr<WebGLBuffer> mBoundArrayBuffer;
    WebGLObjectRefPtr<WebGLBuffer> mBoundElementArrayBuffer;
    WebGLObjectRefPtr<WebGLProgram> mCurrentProgram;

    nsTArray<nsRefPtr<WebGLFramebuffer> > mBoundColorFramebuffers;
    nsRefPtr<WebGLFramebuffer> mBoundDepthFramebuffer;
    nsRefPtr<WebGLFramebuffer> mBoundStencilFramebuffer;

    nsRefPtr<WebGLRenderbuffer> mBoundRenderbuffer;

    // lookup tables for GL name -> object wrapper
    nsRefPtrHashtable<nsUint32HashKey, WebGLTexture> mMapTextures;
    nsRefPtrHashtable<nsUint32HashKey, WebGLBuffer> mMapBuffers;
    nsRefPtrHashtable<nsUint32HashKey, WebGLProgram> mMapPrograms;
    nsRefPtrHashtable<nsUint32HashKey, WebGLShader> mMapShaders;
    nsRefPtrHashtable<nsUint32HashKey, WebGLFramebuffer> mMapFramebuffers;
    nsRefPtrHashtable<nsUint32HashKey, WebGLRenderbuffer> mMapRenderbuffers;

    // console logging helpers
    void LogMessage (const char *fmt, ...);
    nsresult ErrorMessage (const char *fmt, ...);
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

class WebGLBuffer :
    public nsIWebGLBuffer,
    public WebGLZeroingObject
{
public:
    WebGLBuffer(GLuint name)
        : mName(name), mDeleted(PR_FALSE), mGLType(0)
    { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }
    PRBool Deleted() { return mDeleted; }
    GLuint GLName() { return mName; }

    void Set(nsIWebGLArray *na) {
        mGLType = na->NativeType();
        mElementSize = na->NativeElementSize();
        mCount = na->NativeCount();
    }

    GLenum GLType() { return mGLType; }
    PRUint32 ByteCount() { return mElementSize * mCount; }
    PRUint32 Count() { return mCount; }
    PRUint32 ElementSize() { return mElementSize; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLBUFFER
protected:
    GLuint mName;
    PRBool mDeleted;

    GLenum mGLType;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLTexture :
    public nsIWebGLTexture,
    public WebGLZeroingObject
{
public:
    WebGLTexture(GLuint name) :
        mName(name), mDeleted(PR_FALSE) { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }

    PRBool Deleted() { return mDeleted; }
    GLuint GLName() { return mName; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLTEXTURE
protected:
    GLuint mName;
    PRBool mDeleted;
};

class WebGLProgram :
    public nsIWebGLProgram,
    public WebGLZeroingObject
{
public:
    WebGLProgram(GLuint name) :
        mName(name), mDeleted(PR_FALSE) { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }
    PRBool Deleted() { return mDeleted; }
    GLuint GLName() { return mName; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLPROGRAM
protected:
    GLuint mName;
    PRBool mDeleted;
};

class WebGLShader :
    public nsIWebGLShader,
    public WebGLZeroingObject
{
public:
    WebGLShader(GLuint name) :
        mName(name), mDeleted(PR_FALSE) { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }
    PRBool Deleted() { return mDeleted; }
    GLuint GLName() { return mName; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLSHADER
protected:
    GLuint mName;
    PRBool mDeleted;
};

class WebGLFramebuffer :
    public nsIWebGLFramebuffer,
    public WebGLZeroingObject
{
public:
    WebGLFramebuffer(GLuint name) :
        mName(name), mDeleted(PR_FALSE) { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }
    PRBool Deleted() { return mDeleted; }
    GLuint GLName() { return mName; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLFRAMEBUFFER
protected:
    GLuint mName;
    PRBool mDeleted;
};

class WebGLRenderbuffer :
    public nsIWebGLRenderbuffer,
    public WebGLZeroingObject
{
public:
    WebGLRenderbuffer(GLuint name) :
        mName(name), mDeleted(PR_FALSE) { }

    void Delete() {
        if (mDeleted)
            return;
        ZeroOwners();
        mDeleted = PR_TRUE;
    }
    PRBool Deleted() { return mDeleted; }
    GLuint GLName() { return mName; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLRENDERBUFFER
protected:
    GLuint mName;
    PRBool mDeleted;
};

//
// array wrapper classes
//

class WebGLFloatArray :
    public nsIWebGLFloatArray,
    public nsIJSNativeInitializer
{
public:
    WebGLFloatArray();
    WebGLFloatArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLARRAY
    NS_DECL_NSIWEBGLFLOATARRAY

    static nsresult NewCanvasFloatArray(nsISupports **aNewObject);

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);

protected:
    SimpleBuffer mBuffer;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLByteArray :
    public nsIWebGLByteArray,
    public nsIJSNativeInitializer
{
public:
    WebGLByteArray();
    WebGLByteArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLARRAY
    NS_DECL_NSIWEBGLBYTEARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);
protected:
    SimpleBuffer mBuffer;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLUnsignedByteArray :
    public nsIWebGLUnsignedByteArray,
    public nsIJSNativeInitializer
{
public:
    WebGLUnsignedByteArray();
    WebGLUnsignedByteArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLARRAY
    NS_DECL_NSIWEBGLUNSIGNEDBYTEARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);
protected:
    SimpleBuffer mBuffer;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLShortArray :
    public nsIWebGLShortArray,
    public nsIJSNativeInitializer
{
public:
    WebGLShortArray();
    WebGLShortArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLARRAY
    NS_DECL_NSIWEBGLSHORTARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);
protected:
    SimpleBuffer mBuffer;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLUnsignedShortArray :
    public nsIWebGLUnsignedShortArray,
    public nsIJSNativeInitializer
{
public:
    WebGLUnsignedShortArray();
    WebGLUnsignedShortArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLARRAY
    NS_DECL_NSIWEBGLUNSIGNEDSHORTARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);
protected:
    SimpleBuffer mBuffer;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLIntArray :
    public nsIWebGLIntArray,
    public nsIJSNativeInitializer
{
public:
    WebGLIntArray();
    WebGLIntArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLARRAY
    NS_DECL_NSIWEBGLINTARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);
protected:
    SimpleBuffer mBuffer;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

class WebGLUnsignedIntArray :
    public nsIWebGLUnsignedIntArray,
    public nsIJSNativeInitializer
{
public:
    WebGLUnsignedIntArray();
    WebGLUnsignedIntArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBGLARRAY
    NS_DECL_NSIWEBGLUNSIGNEDINTARRAY

    NS_IMETHOD Initialize(nsISupports* aOwner,
                          JSContext* aCx,
                          JSObject* aObj,
                          PRUint32 aArgc,
                          jsval* aArgv);
protected:
    SimpleBuffer mBuffer;
    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mElementSize;
    PRUint32 mCount;
};

}

#endif
