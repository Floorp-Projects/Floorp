/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Orendorff <jorendorff@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef xpcquickstubs_h___
#define xpcquickstubs_h___

/* xpcquickstubs.h - Support functions used only by quick stubs. */

class XPCCallContext;

#define XPC_QS_NULL_INDEX  ((size_t) -1)

struct xpc_qsPropertySpec {
    const char *name;
    JSPropertyOp getter;
    JSPropertyOp setter;
};

struct xpc_qsFunctionSpec {
    const char *name;
    JSFastNative native;
    uintN arity;
};

struct xpc_qsTraceableSpec {
    const char *name;
    JSNative native;
    uintN arity;
};

/** A table mapping interfaces to quick stubs. */
struct xpc_qsHashEntry {
    nsID iid;
    const xpc_qsPropertySpec *properties;
    const xpc_qsFunctionSpec *functions;
    const xpc_qsTraceableSpec *traceables;
    // These last two fields index to other entries in the same table.
    // XPC_QS_NULL_ENTRY indicates there are no more entries in the chain.
    size_t parentInterface;
    size_t chain;
};

JSBool
xpc_qsDefineQuickStubs(JSContext *cx, JSObject *proto, uintN extraFlags,
                       PRUint32 ifacec, const nsIID **interfaces,
                       PRUint32 tableSize, const xpc_qsHashEntry *table);

/** Raise an exception on @a cx and return JS_FALSE. */
JSBool
xpc_qsThrow(JSContext *cx, nsresult rv);

/**
 * Fail after an XPCOM getter or setter returned rv.
 *
 * NOTE: Here @a obj must be the JSObject whose private data field points to an
 * XPCWrappedNative, not merely an object that has an XPCWrappedNative
 * somewhere along the prototype chain!  The same applies to @a obj in
 * xpc_qsThrowBadSetterValue and <code>vp[1]</code> in xpc_qsThrowMethodFailed
 * and xpc_qsThrowBadArg.
 *
 * This is one reason the UnwrapThis functions below have an out parameter that
 * receives the wrapper JSObject.  (The other reason is to help the caller keep
 * that JSObject GC-reachable.)
 */
JSBool
xpc_qsThrowGetterSetterFailed(JSContext *cx, nsresult rv,
                              JSObject *obj, jsval memberId);

/**
 * Fail after an XPCOM method returned rv.
 *
 * See NOTE at xpc_qsThrowGetterSetterFailed.
 */
JSBool
xpc_qsThrowMethodFailed(JSContext *cx, nsresult rv, jsval *vp);

JSBool
xpc_qsThrowMethodFailedWithCcx(XPCCallContext &ccx, nsresult rv);

void
xpc_qsThrowMethodFailedWithDetails(JSContext *cx, nsresult rv,
                                   const char *ifaceName,
                                   const char *memberName);

/**
 * Fail after converting a method argument fails.
 *
 * See NOTE at xpc_qsThrowGetterSetterFailed.
 */
void
xpc_qsThrowBadArg(JSContext *cx, nsresult rv, jsval *vp, uintN paramnum);

void
xpc_qsThrowBadArgWithCcx(XPCCallContext &ccx, nsresult rv, uintN paramnum);

void
xpc_qsThrowBadArgWithDetails(JSContext *cx, nsresult rv, uintN paramnum,
                             const char *ifaceName, const char *memberName);

/**
 * Fail after converting a setter argument fails.
 *
 * See NOTE at xpc_qsThrowGetterSetterFailed.
 */
void
xpc_qsThrowBadSetterValue(JSContext *cx, nsresult rv, JSObject *obj,
                          jsval propId);


JSBool
xpc_qsGetterOnlyPropertyStub(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

/* Functions for converting values between COM and JS. */

inline JSBool
xpc_qsInt32ToJsval(JSContext *cx, PRInt32 i, jsval *rv)
{
    if(INT_FITS_IN_JSVAL(i))
    {
        *rv = INT_TO_JSVAL(i);
        return JS_TRUE;
    }
    return JS_NewDoubleValue(cx, i, rv);
}

inline JSBool
xpc_qsUint32ToJsval(JSContext *cx, PRUint32 u, jsval *rv)
{
    if(u <= JSVAL_INT_MAX)
    {
        *rv = INT_TO_JSVAL(u);
        return JS_TRUE;
    }
    return JS_NewDoubleValue(cx, u, rv);
}

#ifdef HAVE_LONG_LONG

#define INT64_TO_DOUBLE(i)      ((jsdouble) (i))
// Win32 can't handle uint64 to double conversion
#define UINT64_TO_DOUBLE(u)     ((jsdouble) (int64) (u))

#else

inline jsdouble
INT64_TO_DOUBLE(const int64 &v)
{
    jsdouble d;
    LL_L2D(d, v);
    return d;
}

// if !HAVE_LONG_LONG, then uint64 is a typedef of int64
#define UINT64_TO_DOUBLE INT64_TO_DOUBLE

#endif

inline JSBool
xpc_qsInt64ToJsval(JSContext *cx, PRInt64 i, jsval *rv)
{
    double d = INT64_TO_DOUBLE(i);
    return JS_NewNumberValue(cx, d, rv);
}

inline JSBool
xpc_qsUint64ToJsval(JSContext *cx, PRUint64 u, jsval *rv)
{
    double d = UINT64_TO_DOUBLE(u);
    return JS_NewNumberValue(cx, d, rv);
}


/* Classes for converting jsvals to string types. */

template <class S, class T>
class xpc_qsBasicString
{
public:
    typedef S interface_type;
    typedef T implementation_type;

    ~xpc_qsBasicString()
    {
        if (mValid)
            Ptr()->~implementation_type();
    }

    JSBool IsValid() { return mValid; }

    implementation_type *Ptr()
    {
        return reinterpret_cast<implementation_type *>(mBuf);
    }

    operator interface_type &()
    {
        return *Ptr();
    }

protected:
    /*
     * Neither field is initialized; that is left to the derived class
     * constructor. However, the destructor destroys the string object
     * stored in mBuf, if mValid is true.
     */
    void *mBuf[JS_HOWMANY(sizeof(implementation_type), sizeof(void *))];
    JSBool mValid;
};

/**
 * Class for converting a jsval to DOMString.
 *
 *     xpc_qsDOMString arg0(cx, &argv[0]);
 *     if (!arg0.IsValid())
 *         return JS_FALSE;
 *
 * The second argument to the constructor is an in-out parameter. It must
 * point to a rooted jsval, such as a JSNative argument or return value slot.
 * The value in the jsval on entry is converted to a string. The constructor
 * may overwrite that jsval with a string value, to protect the characters of
 * the string from garbage collection. The caller must leave the jsval alone
 * for the lifetime of the xpc_qsDOMString.
 */
class xpc_qsDOMString : public xpc_qsBasicString<nsAString, nsDependentString>
{
public:
    /* Enum that defines how JS |null| and |undefined| should be treated.  See
     * the WebIDL specification.  eStringify means convert to the string "null"
     * or "undefined" respectively, via the standard JS ToString() operation;
     * eEmpty means convert to the string ""; eNull means convert to an empty
     * string with the void bit set.
     *
     * Per webidl the default behavior of an unannotated interface is
     * eStringify, but our de-facto behavior has been eNull for |null| and
     * eStringify for |undefined|, so leaving it that way for now.  If we ever
     * get to a point where we go through and annotate our interfaces as
     * needed, we can change that.
     */
    enum StringificationBehavior {
        eStringify,
        eEmpty,
        eNull,
        eDefaultNullBehavior = eNull,
        eDefaultUndefinedBehavior = eStringify
    };

    xpc_qsDOMString(JSContext *cx, jsval v, jsval *pval,
                    StringificationBehavior nullBehavior,
                    StringificationBehavior undefinedBehavior);
};

/**
 * The same as xpc_qsDOMString, but with slightly different conversion behavior,
 * corresponding to the [astring] magic XPIDL annotation rather than [domstring].
 */
class xpc_qsAString : public xpc_qsDOMString
{
public:
    xpc_qsAString(JSContext *cx, jsval v, jsval *pval)
        : xpc_qsDOMString(cx, v, pval, eNull, eNull)
    {}
};

/**
 * Like xpc_qsDOMString and xpc_qsAString, but for XPIDL native types annotated
 * with [cstring] rather than [domstring] or [astring].
 */
class xpc_qsACString : public xpc_qsBasicString<nsACString, nsCString>
{
public:
    xpc_qsACString(JSContext *cx, jsval v, jsval *pval);
};

struct xpc_qsSelfRef
{
    xpc_qsSelfRef() : ptr(nsnull) {}
    explicit xpc_qsSelfRef(nsISupports *p) : ptr(p) {}
    ~xpc_qsSelfRef() { NS_IF_RELEASE(ptr); }

    nsISupports* ptr;
};

template<size_t N>
struct xpc_qsArgValArray
{
    xpc_qsArgValArray(JSContext *cx) : tvr(cx, N, array)
    {
        memset(array, 0, N * sizeof(jsval));
    }

    js::AutoArrayRooter tvr;
    jsval array[N];
};

/**
 * Convert a jsval to char*, returning JS_TRUE on success.
 *
 * @param cx
 *      A context.
 * @param pval
 *     In/out. *pval is the jsval to convert; the function may write to *pval,
 *     using it as a GC root (like xpc_qsDOMString's constructor).
 * @param pstr
 *     Out. On success *pstr receives the converted string or NULL if *pval is
 *     null or undefined. Unicode data is garbled as with JS_GetStringBytes.
 */
JSBool
xpc_qsJsvalToCharStr(JSContext *cx, jsval v, jsval *pval, char **pstr);

JSBool
xpc_qsJsvalToWcharStr(JSContext *cx, jsval v, jsval *pval, PRUnichar **pstr);


/** Convert an nsAString to jsval, returning JS_TRUE on success. */
JSBool
xpc_qsStringToJsval(JSContext *cx, const nsAString &str, jsval *rval);

nsresult
getWrapper(JSContext *cx,
           JSObject *obj,
           JSObject *callee,
           XPCWrappedNative **wrapper,
           JSObject **cur,
           XPCWrappedNativeTearOff **tearoff);

nsresult
castNative(JSContext *cx,
           XPCWrappedNative *wrapper,
           JSObject *cur,
           XPCWrappedNativeTearOff *tearoff,
           const nsIID &iid,
           void **ppThis,
           nsISupports **ppThisRef,
           jsval *vp,
           XPCLazyCallContext *lccx);

/**
 * Search @a obj and its prototype chain for an XPCOM object that implements
 * the interface T.
 *
 * If an object implementing T is found, store a reference to the wrapper
 * JSObject in @a *pThisVal, store a pointer to the T in @a *ppThis, and return
 * JS_TRUE. Otherwise, raise an exception on @a cx and return JS_FALSE.
 *
 * @a *pThisRef receives the same pointer as *ppThis if the T was AddRefed.
 * Otherwise it receives null (even on error).
 *
 * This supports split objects and XPConnect tear-offs and it sees through
 * XOWs, XPCNativeWrappers, and SafeJSObjectWrappers.
 *
 * Requires a request on @a cx.
 */
template <class T>
inline JSBool
xpc_qsUnwrapThis(JSContext *cx,
                 JSObject *obj,
                 JSObject *callee,
                 T **ppThis,
                 nsISupports **pThisRef,
                 jsval *pThisVal,
                 XPCLazyCallContext *lccx)
{
    XPCWrappedNative *wrapper;
    XPCWrappedNativeTearOff *tearoff;
    nsresult rv = getWrapper(cx, obj, callee, &wrapper, &obj, &tearoff);
    if(NS_SUCCEEDED(rv))
        rv = castNative(cx, wrapper, obj, tearoff, NS_GET_TEMPLATE_IID(T),
                        reinterpret_cast<void **>(ppThis), pThisRef, pThisVal,
                        lccx);

    return NS_SUCCEEDED(rv) || xpc_qsThrow(cx, rv);
}

inline nsISupports*
castNativeFromWrapper(JSContext *cx,
                      JSObject *obj,
                      JSObject *callee,
                      PRUint32 interfaceBit,
                      nsISupports **pRef,
                      jsval *pVal,
                      XPCLazyCallContext *lccx,
                      nsresult *rv NS_OUTPARAM)
{
    XPCWrappedNative *wrapper;
    XPCWrappedNativeTearOff *tearoff;
    JSObject *cur;

    if(!callee && IS_WRAPPER_CLASS(obj->getClass()))
    {
        cur = obj;
        wrapper = IS_WN_WRAPPER_OBJECT(cur) ?
                  (XPCWrappedNative*)xpc_GetJSPrivate(obj) :
                  nsnull;
        tearoff = nsnull;
    }
    else
    {
        *rv = getWrapper(cx, obj, callee, &wrapper, &cur, &tearoff);
        if (NS_FAILED(*rv))
            return nsnull;
    }

    nsISupports *native;
    if(wrapper)
    {
        native = wrapper->GetIdentityObject();
        cur = wrapper->GetFlatJSObject();
    }
    else
    {
        native = cur ?
                 static_cast<nsISupports*>(xpc_GetJSPrivate(cur)) :
                 nsnull;
    }

    *rv = NS_ERROR_XPC_BAD_CONVERT_JS;

    if(!native)
        return nsnull;

    NS_ASSERTION(IS_WRAPPER_CLASS(cur->getClass()), "Not a wrapper?");

    XPCNativeScriptableSharedJSClass *clasp =
      (XPCNativeScriptableSharedJSClass*)cur->getClass();
    if(!(clasp->interfacesBitmap & (1 << interfaceBit)))
        return nsnull;

    *pRef = nsnull;
    *pVal = OBJECT_TO_JSVAL(cur);

    if(lccx)
    {
        if(wrapper)
            lccx->SetWrapper(wrapper, tearoff);
        else
            lccx->SetWrapper(cur);
    }

    *rv = NS_OK;

    return native;
}

JSBool
xpc_qsUnwrapThisFromCcxImpl(XPCCallContext &ccx,
                            const nsIID &iid,
                            void **ppThis,
                            nsISupports **pThisRef,
                            jsval *vp);

/**
 * Alternate implementation of xpc_qsUnwrapThis using information already
 * present in the given XPCCallContext.
 */
template <class T>
inline JSBool
xpc_qsUnwrapThisFromCcx(XPCCallContext &ccx,
                        T **ppThis,
                        nsISupports **pThisRef,
                        jsval *pThisVal)
{
    return xpc_qsUnwrapThisFromCcxImpl(ccx,
                                       NS_GET_TEMPLATE_IID(T),
                                       reinterpret_cast<void **>(ppThis),
                                       pThisRef,
                                       pThisVal);
}

JSObject*
xpc_qsUnwrapObj(jsval v, nsISupports **ppArgRef, nsresult *rv);

nsresult
xpc_qsUnwrapArgImpl(JSContext *cx, jsval v, const nsIID &iid, void **ppArg,
                    nsISupports **ppArgRef, jsval *vp);

/** Convert a jsval to an XPCOM pointer. */
template <class T>
inline nsresult
xpc_qsUnwrapArg(JSContext *cx, jsval v, T **ppArg, nsISupports **ppArgRef,
                jsval *vp)
{
    return xpc_qsUnwrapArgImpl(cx, v, NS_GET_TEMPLATE_IID(T),
                               reinterpret_cast<void **>(ppArg), ppArgRef, vp);
}

inline nsISupports*
castNativeArgFromWrapper(JSContext *cx,
                         jsval v,
                         PRUint32 bit,
                         nsISupports **pArgRef,
                         jsval *vp,
                         nsresult *rv NS_OUTPARAM)
{
    JSObject *src = xpc_qsUnwrapObj(v, pArgRef, rv);
    if(!src)
        return nsnull;

    return castNativeFromWrapper(cx, src, nsnull, bit, pArgRef, vp, nsnull, rv);
}

inline nsWrapperCache*
xpc_qsGetWrapperCache(nsWrapperCache *cache)
{
    return cache;
}

inline nsWrapperCache*
xpc_qsGetWrapperCache(void *p)
{
    return nsnull;
}

/** Convert an XPCOM pointer to jsval. Return JS_TRUE on success. */
JSBool
xpc_qsXPCOMObjectToJsval(XPCLazyCallContext &lccx,
                         nsISupports *p,
                         nsWrapperCache *cache,
                         const nsIID *iid,
                         XPCNativeInterface **iface,
                         jsval *rval);

/**
 * Convert a variant to jsval. Return JS_TRUE on success.
 */
JSBool
xpc_qsVariantToJsval(XPCLazyCallContext &ccx,
                     nsIVariant *p,
                     jsval *rval);

inline nsISupports*
ToSupports(nsISupports *p)
{
    return p;
}

#ifdef DEBUG
void
xpc_qsAssertContextOK(JSContext *cx);

inline PRBool
xpc_qsSameResult(nsISupports *result1, nsISupports *result2)
{
    return SameCOMIdentity(result1, result2);
}

inline PRBool
xpc_qsSameResult(const nsString &result1, const nsString &result2)
{
    return result1.Equals(result2);
}

inline PRBool
xpc_qsSameResult(PRInt32 result1, PRInt32 result2)
{
    return result1 == result2;
}

#define XPC_QS_ASSERT_CONTEXT_OK(cx) xpc_qsAssertContextOK(cx)
#else
#define XPC_QS_ASSERT_CONTEXT_OK(cx) ((void) 0)
#endif

#endif /* xpcquickstubs_h___ */
