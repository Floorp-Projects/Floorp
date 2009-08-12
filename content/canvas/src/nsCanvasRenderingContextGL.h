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

#ifndef _NSCANVASRENDERINGCONTEXTGL_H_
#define _NSCANVASRENDERINGCONTEXTGL_H_

#ifdef C3D_STANDALONE_BUILD
#include "c3d-standalone.h"
#endif

#include "nsICanvasRenderingContextGL.h"

#include <stdlib.h>
#include <stdarg.h>

#include "prmem.h"

#include "nsStringGlue.h"

#include "nsICanvasRenderingContextGLBuffer.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsIDOMHTMLCanvasElement.h"

#include "nsICanvasGLPrivate.h"

#include "nsIScriptSecurityManager.h"
#include "nsISecurityCheckedComponent.h"

#include "nsWeakReference.h"

#include "imgIRequest.h"
#include "imgIContainer.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsICanvasElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIImageLoadingContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsDOMError.h"
#include "nsIJSRuntimeService.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"

#include "nsDOMError.h"

#include "nsServiceManagerUtils.h"

#include "nsIXPConnect.h"
#include "jsapi.h"

#include "gfxContext.h"

#include "nsGLPbuffer.h"

extern nsIXPConnect *gXPConnect;
extern JSRuntime *gScriptRuntime;
extern nsIJSRuntimeService *gJSRuntimeService;

class nsICanvasRenderingContextGL;

class nsCanvasRenderingContextGLES11;
class nsCanvasRenderingContextGLWeb20;

class CanvasGLBuffer;
class CanvasGLTexture;

class nsCanvasRenderingContextGLPrivate :
    public nsICanvasRenderingContextInternal,
    public nsSupportsWeakReference
{
    friend class nsGLPbuffer;
    friend class CanvasGLBuffer;
    friend class CanvasGLTexture;

public:
    nsCanvasRenderingContextGLPrivate();
    virtual ~nsCanvasRenderingContextGLPrivate();

    virtual nsICanvasRenderingContextGL *GetSelf() = 0;

    virtual PRBool ValidateGL() { return PR_TRUE; }

    void MakeContextCurrent();
    static void LostCurrentContext(void *closure);

    // nsICanvasRenderingContextInternal
    NS_IMETHOD SetCanvasElement(nsICanvasElement* aParentCanvas);
    NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height);
    NS_IMETHOD Render(gfxContext *ctx, gfxPattern::GraphicsFilter f);
    NS_IMETHOD GetInputStream(const char* aMimeType,
                              const PRUnichar* aEncoderOptions,
                              nsIInputStream **aStream);
    NS_IMETHOD GetThebesSurface(gfxASurface **surface);
    NS_IMETHOD SetIsOpaque(PRBool b) { return NS_OK; };

protected:
    PRBool SafeToCreateCanvas3DContext(nsICanvasElement *canvasElement);
    nsresult DoSwapBuffers();

    // thebes helpers
    nsresult ImageSurfaceFromElement(nsIDOMElement *imgElt,
                                     gfxImageSurface **aSurface,
                                     nsIPrincipal **prinOut,
                                     PRBool *forceWriteOnlyOut,
                                     PRBool *surfaceNeedsReleaseInsteadOfDelete);

    void DoDrawImageSecurityCheck(nsIPrincipal* element_uri, PRBool forceWriteOnly);

    GLES20Wrap *gl;

    nsGLPbuffer *mGLPbuffer;
    PRInt32 mWidth, mHeight;
    nsICanvasElement* mCanvasElement;

    PRPackedBool mPrefWireframe;

    void LogMessage (const nsCString& errorString) {
        nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
        if (!console)
            return;

        console->LogStringMessage(NS_ConvertUTF8toUTF16(errorString).get());
        fprintf(stderr, "%s\n", errorString.get());
    }

    void LogMessagef (const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        char buf[256];

        nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
        if (console) {
            vsnprintf(buf, 256, fmt, ap);
            console->LogStringMessage(NS_ConvertUTF8toUTF16(nsDependentCString(buf)).get());
            fprintf(stderr, "%s\n", buf);
        }

        va_end(ap);
    }

};

class SimpleBuffer {
public:
    SimpleBuffer()
      : type(GL_FLOAT), data(nsnull), length(0), capacity(0), sizePerVertex(0)
    { }

    SimpleBuffer(PRUint32 typeParam,
                 PRUint32 sizeParam,
                 JSContext *ctx,
                 JSObject *arrayObj,
                 jsuint arrayLen)
      : type(GL_FLOAT), data(nsnull), length(0), capacity(0), sizePerVertex(0)
    {
        InitFromJSArray(typeParam, sizeParam, ctx, arrayObj, arrayLen);
    }

    PRBool InitFromJSArray(PRUint32 typeParam,
                           PRUint32 sizeParam,
                           JSContext *ctx,
                           JSObject *arrayObj,
                           jsuint arrayLen);

    ~SimpleBuffer() {
        Release();
    }

    inline PRBool Valid() {
        return data != nsnull;
    }

    inline PRUint32 ElementSize() {
        if (type == GL_FLOAT) return sizeof(float);
        if (type == GL_SHORT) return sizeof(short);
        if (type == GL_UNSIGNED_SHORT) return sizeof(unsigned short);
        if (type == GL_BYTE) return 1;
        if (type == GL_UNSIGNED_BYTE) return 1;
        if (type == GL_INT) return sizeof(int);
        if (type == GL_UNSIGNED_INT) return sizeof(unsigned int);
        if (type == GL_DOUBLE) return sizeof(double);
        return 0;
    }

    void Clear() {
        Release();
    }

    void Set(PRUint32 t, PRUint32 spv, PRUint32 count, void* vals) {
        Prepare(t, spv, count);

        if (count)
            memcpy(data, vals, count*ElementSize());
    }

    void Prepare(PRUint32 t, PRUint32 spv, PRUint32 count) {
        if (count == 0) {
            Release();
        } else {
            type = t;
            EnsureCapacity(PR_FALSE, count*ElementSize());
            length = count;
            sizePerVertex = spv;
        }
    }

    void Release() {
        if (data)
            PR_Free(data);
        length = 0;
        capacity = 0;
        data = nsnull;
    }

    void EnsureCapacity(PRBool preserve, PRUint32 cap) {
        if (capacity >= cap)
            return;

        void* newdata = PR_Malloc(cap);
        if (preserve && length)
            memcpy(newdata, data, length*ElementSize());
        PR_Free(data);
        data = newdata;
        capacity = cap;
    }

    PRUint32 type;
    void* data;
    PRUint32 length;        // # of elements
    PRUint32 capacity;      // bytes!
    PRUint32 sizePerVertex; // OpenGL "size" param; num coordinates per vertex
};

class CanvasGLTexture :
    public nsICanvasRenderingContextGLTexture,
    public nsICanvasGLTexture
{
    friend class nsCanvasRenderingContextGLES11;
    friend class nsCanvasRenderingContextGLWeb20;
public:
    CanvasGLTexture(nsCanvasRenderingContextGLPrivate *owner);
    ~CanvasGLTexture();

    NS_DECL_ISUPPORTS

    NS_DECL_NSICANVASRENDERINGCONTEXTGLTEXTURE

    nsresult Init();
    nsresult Dispose();

protected:
    PRBool mDisposed;
    nsCOMPtr<nsIWeakReference> mOwnerContext;

    GLES20Wrap *gl;

    PRUint32 mWidth;
    PRUint32 mHeight;
};

class CanvasGLBuffer :
    public nsICanvasRenderingContextGLBuffer,
    public nsISecurityCheckedComponent,
    public nsICanvasGLBuffer
{
    friend class nsCanvasRenderingContextGLES11;
    friend class nsCanvasRenderingContextGLWeb20;
public:

    CanvasGLBuffer(nsCanvasRenderingContextGLPrivate *owner);
    ~CanvasGLBuffer();

    // Init can be called multiple times to reinitialize this
    // buffer object
    nsresult Init (PRUint32 usage,
                   PRUint32 size,
                   PRUint32 type,
                   JSContext *ctx,
                   JSObject *arrayObj,
                   jsuint arrayLen);

    SimpleBuffer& GetSimpleBuffer() { return mSimpleBuffer; }
    PRBool UpdateBuffer (PRUint32 offset, SimpleBuffer& sbuffer)
    {
        PRUint32 len = GetSimpleBuffer().capacity;
        PRUint32 sbuflen = sbuffer.capacity;
        if (offset < 0 || offset > len || sbuflen > len || offset > len - sbuflen)
            return false;
        memcpy(((char*)(GetSimpleBuffer().data)) + offset, sbuffer.data, sbuflen);
        mMaxUShortComputed = false;
        return true;
    }
    GLushort MaxUShortValue()
    {
        if (!mMaxUShortComputed) {
            GLushort *data = (GLushort*)GetSimpleBuffer().data;
            PRUint32 i, len;
            GLushort max = 0;
            len = GetSimpleBuffer().capacity / sizeof(GLushort);
            for (i=0; i<len; ++i)
                if (data[i] > max)
                    max = data[i];
            mMaxUShort = max;
            mMaxUShortComputed = true;
        }
        return mMaxUShort;
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSICANVASRENDERINGCONTEXTGLBUFFER
    NS_DECL_NSISECURITYCHECKEDCOMPONENT

    PRUint32 Size() { return mSize; }
    PRUint32 Length() { return mLength; }
    PRUint32 Type() { return mType; }

protected:
    CanvasGLBuffer() { }

    nsCOMPtr<nsIWeakReference> mOwnerContext;

    GLES20Wrap *gl;

    PRBool mDisposed;

    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mType;
    PRUint32 mUsage;

    SimpleBuffer mSimpleBuffer;
    GLuint mBufferID;

    GLushort mMaxUShort;
    PRBool mMaxUShortComputed;
};

class CanvasGLThebes {
public:
    static gfxImageSurface *CreateImageSurface (const gfxIntSize &isize,
                                                gfxASurface::gfxImageFormat fmt);

    static gfxContext *CreateContext (gfxASurface *surf);

    static gfxPattern *CreatePattern (gfxASurface *surf);
};

/* Helper macros for when we're just wrapping a gl method, so that
 * we can avoid having to type this 500 times.  Note that these MUST
 * NOT BE USED if we need to check any of the parameters.
 */

#define GL_SAME_METHOD_0(glname, name)                       \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name() {       \
    MakeContextCurrent(); gl->f##glname(); return NS_OK;        \
}

#define GL_SAME_METHOD_1(glname, name, t1)                            \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1) {           \
    MakeContextCurrent(); gl->f##glname(a1); return NS_OK;               \
}

#define GL_SAME_METHOD_2(glname, name, t1, t2)                          \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1, t2 a2) {      \
    MakeContextCurrent(); gl->f##glname(a1,a2); return NS_OK;              \
}

#define GL_SAME_METHOD_3(glname, name, t1, t2, t3)                        \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1, t2 a2, t3 a3) { \
    MakeContextCurrent(); gl->f##glname(a1,a2,a3); return NS_OK;             \
}

#define GL_SAME_METHOD_4(glname, name, t1, t2, t3, t4)                           \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1, t2 a2, t3 a3, t4 a4) { \
    MakeContextCurrent(); gl->f##glname(a1,a2,a3,a4); return NS_OK;                 \
}

#define GL_SAME_METHOD_5(glname, name, t1, t2, t3, t4, t5)                              \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) { \
    MakeContextCurrent(); gl->f##glname(a1,a2,a3,a4,a5); return NS_OK;                     \
}

#define GL_SAME_METHOD_6(glname, name, t1, t2, t3, t4, t5, t6)                                 \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) { \
    MakeContextCurrent(); gl->f##glname(a1,a2,a3,a4,a5,a6); return NS_OK;                         \
}

#endif /* _NSCANVASRENDERINGCONTEXTGL_H_ */
