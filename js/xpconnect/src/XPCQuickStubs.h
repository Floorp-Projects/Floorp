/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcquickstubs_h___
#define xpcquickstubs_h___

#include "xpcpublic.h"
#include "xpcprivate.h"
#include "qsObjectHelper.h"

#include "jsatom.h"

/* XPCQuickStubs.h - Support functions used only by quick stubs. */

class XPCCallContext;

#define XPC_QS_NULL_INDEX  ((uint16_t) -1)

struct xpc_qsPropertySpec {
    uint16_t name_index;
    JSPropertyOp getter;
    JSStrictPropertyOp setter;
};

struct xpc_qsFunctionSpec {
    uint16_t name_index;
    uint16_t arity;
    JSNative native;
};

/** A table mapping interfaces to quick stubs. */
struct xpc_qsHashEntry {
    nsID iid;
    uint16_t prop_index;
    uint16_t n_props;
    uint16_t func_index;
    uint16_t n_funcs;
    // These last two fields index to other entries in the same table.
    // XPC_QS_NULL_ENTRY indicates there are no more entries in the chain.
    uint16_t parentInterface;
    uint16_t chain;
};

JSBool
xpc_qsDefineQuickStubs(JSContext *cx, JSObject *proto, unsigned extraFlags,
                       PRUint32 ifacec, const nsIID **interfaces,
                       PRUint32 tableSize, const xpc_qsHashEntry *table,
                       const xpc_qsPropertySpec *propspecs,
                       const xpc_qsFunctionSpec *funcspecs,
                       const char *stringTable);

/** Raise an exception on @a cx and return false. */
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
                              JSObject *obj, jsid memberId);

/**
 * Fail after an XPCOM method returned rv.
 *
 * See NOTE at xpc_qsThrowGetterSetterFailed.
 */
JSBool
xpc_qsThrowMethodFailed(JSContext *cx, nsresult rv, jsval *vp);

JSBool
xpc_qsThrowMethodFailedWithCcx(XPCCallContext &ccx, nsresult rv);

bool
xpc_qsThrowMethodFailedWithDetails(JSContext *cx, nsresult rv,
                                   const char *ifaceName,
                                   const char *memberName);

/**
 * Fail after converting a method argument fails.
 *
 * See NOTE at xpc_qsThrowGetterSetterFailed.
 */
void
xpc_qsThrowBadArg(JSContext *cx, nsresult rv, jsval *vp, unsigned paramnum);

void
xpc_qsThrowBadArgWithCcx(XPCCallContext &ccx, nsresult rv, unsigned paramnum);

void
xpc_qsThrowBadArgWithDetails(JSContext *cx, nsresult rv, unsigned paramnum,
                             const char *ifaceName, const char *memberName);

/**
 * Fail after converting a setter argument fails.
 *
 * See NOTE at xpc_qsThrowGetterSetterFailed.
 */
void
xpc_qsThrowBadSetterValue(JSContext *cx, nsresult rv, JSObject *obj,
                          jsid propId);


JSBool
xpc_qsGetterOnlyPropertyStub(JSContext *cx, JSHandleObject obj, JSHandleId id, JSBool strict, jsval *vp);

/* Functions for converting values between COM and JS. */

inline JSBool
xpc_qsInt64ToJsval(JSContext *cx, PRInt64 i, jsval *rv)
{
    return JS_NewNumberValue(cx, static_cast<double>(i), rv);
}

inline JSBool
xpc_qsUint64ToJsval(JSContext *cx, PRUint64 u, jsval *rv)
{
    return JS_NewNumberValue(cx, static_cast<double>(u), rv);
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

    JSBool IsValid() const { return mValid; }

    implementation_type *Ptr()
    {
        MOZ_ASSERT(mValid);
        return reinterpret_cast<implementation_type *>(mBuf);
    }

    const implementation_type *Ptr() const
    {
        MOZ_ASSERT(mValid);
        return reinterpret_cast<const implementation_type *>(mBuf);
    }

    operator interface_type &()
    {
        MOZ_ASSERT(mValid);
        return *Ptr();
    }

    operator const interface_type &() const
    {
        MOZ_ASSERT(mValid);
        return *Ptr();
    }

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

protected:
    /*
     * Neither field is initialized; that is left to the derived class
     * constructor. However, the destructor destroys the string object
     * stored in mBuf, if mValid is true.
     */
    void *mBuf[JS_HOWMANY(sizeof(implementation_type), sizeof(void *))];
    JSBool mValid;

    /*
     * If null is returned, then we either failed or fully initialized
     * |this|; in either case the caller should return immediately
     * without doing anything else. Otherwise, the JSString* created
     * from |v| will be returned.  It'll be rooted, as needed, in
     * *pval.  nullBehavior and undefinedBehavior control what happens
     * when |v| is JSVAL_IS_NULL and JSVAL_IS_VOID respectively.
     */
    template<class traits>
    JSString* InitOrStringify(JSContext* cx, jsval v, jsval* pval,
                              StringificationBehavior nullBehavior,
                              StringificationBehavior undefinedBehavior) {
        JSString *s;
        if (JSVAL_IS_STRING(v)) {
            s = JSVAL_TO_STRING(v);
        } else {
            StringificationBehavior behavior = eStringify;
            if (JSVAL_IS_NULL(v)) {
                behavior = nullBehavior;
            } else if (JSVAL_IS_VOID(v)) {
                behavior = undefinedBehavior;
            }

            // If pval is null, that means the argument was optional and
            // not passed; turn those into void strings if they're
            // supposed to be stringified.
            if (behavior != eStringify || !pval) {
                // Here behavior == eStringify implies !pval, so both eNull and
                // eStringify should end up with void strings.
                (new(mBuf) implementation_type(traits::sEmptyBuffer, PRUint32(0)))->
                    SetIsVoid(behavior != eEmpty);
                mValid = true;
                return nsnull;
            }

            s = JS_ValueToString(cx, v);
            if (!s) {
                mValid = false;
                return nsnull;
            }
            *pval = STRING_TO_JSVAL(s);  // Root the new string.
        }

        return s;
    }
};

/**
 * Class for converting a jsval to DOMString.
 *
 *     xpc_qsDOMString arg0(cx, &argv[0]);
 *     if (!arg0.IsValid())
 *         return false;
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
    xpc_qsACString(JSContext *cx, jsval v, jsval *pval,
                   StringificationBehavior nullBehavior = eNull,
                   StringificationBehavior undefinedBehavior = eNull);
};

/**
 * And similar for AUTF8String.
 */
class xpc_qsAUTF8String :
  public xpc_qsBasicString<nsACString, NS_ConvertUTF16toUTF8>
{
public:
  xpc_qsAUTF8String(JSContext* cx, jsval v, jsval *pval);
};

struct xpc_qsSelfRef
{
    xpc_qsSelfRef() : ptr(nsnull) {}
    explicit xpc_qsSelfRef(nsISupports *p) : ptr(p) {}
    ~xpc_qsSelfRef() { NS_IF_RELEASE(ptr); }

    nsISupports* ptr;
};

/**
 * Convert a jsval to char*, returning true on success.
 *
 * @param cx
 *     A context.
 * @param v
 *     A value to convert.
 * @param bytes
 *     Out. On success it receives the converted string unless v is null or
 *     undefinedin which case bytes->ptr() remains null.
 */
JSBool
xpc_qsJsvalToCharStr(JSContext *cx, jsval v, JSAutoByteString *bytes);

JSBool
xpc_qsJsvalToWcharStr(JSContext *cx, jsval v, jsval *pval, const PRUnichar **pstr);


/** Convert an nsString to JSString, returning true on success. This will sometimes modify |str| to be empty. */
JSBool
xpc_qsStringToJsstring(JSContext *cx, nsString &str, JSString **rval);

nsresult
getWrapper(JSContext *cx,
           JSObject *obj,
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
 * true. Otherwise, raise an exception on @a cx and return false.
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
                 T **ppThis,
                 nsISupports **pThisRef,
                 jsval *pThisVal,
                 XPCLazyCallContext *lccx,
                 bool failureFatal = true)
{
    XPCWrappedNative *wrapper;
    XPCWrappedNativeTearOff *tearoff;
    nsresult rv = getWrapper(cx, obj, &wrapper, &obj, &tearoff);
    if (NS_SUCCEEDED(rv))
        rv = castNative(cx, wrapper, obj, tearoff, NS_GET_TEMPLATE_IID(T),
                        reinterpret_cast<void **>(ppThis), pThisRef, pThisVal,
                        lccx);

    if (failureFatal)
        return NS_SUCCEEDED(rv) || xpc_qsThrow(cx, rv);

    if (NS_FAILED(rv))
        *ppThis = nsnull;
    return true;
}

inline nsISupports*
castNativeFromWrapper(JSContext *cx,
                      JSObject *obj,
                      PRUint32 interfaceBit,
                      nsISupports **pRef,
                      jsval *pVal,
                      XPCLazyCallContext *lccx,
                      nsresult *rv NS_OUTPARAM)
{
    XPCWrappedNative *wrapper;
    XPCWrappedNativeTearOff *tearoff;
    JSObject *cur;

    if (IS_WRAPPER_CLASS(js::GetObjectClass(obj))) {
        cur = obj;
        wrapper = IS_WN_WRAPPER_OBJECT(cur) ?
                  (XPCWrappedNative*)xpc_GetJSPrivate(obj) :
                  nsnull;
        tearoff = nsnull;
    } else {
        *rv = getWrapper(cx, obj, &wrapper, &cur, &tearoff);
        if (NS_FAILED(*rv))
            return nsnull;
    }

    nsISupports *native;
    if (wrapper) {
        native = wrapper->GetIdentityObject();
        cur = wrapper->GetFlatJSObject();
    } else if (cur && IS_SLIM_WRAPPER(cur)) {
        native = static_cast<nsISupports*>(xpc_GetJSPrivate(cur));
    } else {
        native = nsnull;
    }

    *rv = NS_ERROR_XPC_BAD_CONVERT_JS;

    if (!native)
        return nsnull;

    NS_ASSERTION(IS_WRAPPER_CLASS(js::GetObjectClass(cur)), "Not a wrapper?");

    XPCWrappedNativeJSClass *clasp =
      (XPCWrappedNativeJSClass*)js::GetObjectClass(cur);
    if (!(clasp->interfacesBitmap & (1 << interfaceBit)))
        return nsnull;

    *pRef = nsnull;
    *pVal = OBJECT_TO_JSVAL(cur);

    if (lccx) {
        if (wrapper)
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
template <class Interface, class StrongRefType>
inline nsresult
xpc_qsUnwrapArg(JSContext *cx, jsval v, Interface **ppArg,
                StrongRefType **ppArgRef, jsval *vp)
{
    nsISupports* argRef = *ppArgRef;
    nsresult rv = xpc_qsUnwrapArgImpl(cx, v, NS_GET_TEMPLATE_IID(Interface),
                                      reinterpret_cast<void **>(ppArg), &argRef,
                                      vp);
    *ppArgRef = static_cast<StrongRefType*>(argRef);
    return rv;
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
    if (!src)
        return nsnull;

    return castNativeFromWrapper(cx, src, bit, pArgRef, vp, nsnull, rv);
}

inline nsWrapperCache*
xpc_qsGetWrapperCache(nsWrapperCache *cache)
{
    return cache;
}

// nsGlobalWindow implements nsWrapperCache, but doesn't always use it. Don't
// try to use it without fixing that first.
class nsGlobalWindow;
inline nsWrapperCache*
xpc_qsGetWrapperCache(nsGlobalWindow *not_allowed);

inline nsWrapperCache*
xpc_qsGetWrapperCache(void *p)
{
    return nsnull;
}

/** Convert an XPCOM pointer to jsval. Return true on success.
 * aIdentity is a performance optimization. Set it to true,
 * only if p is the identity pointer.
 */
JSBool
xpc_qsXPCOMObjectToJsval(XPCLazyCallContext &lccx,
                         qsObjectHelper &aHelper,
                         const nsIID *iid,
                         XPCNativeInterface **iface,
                         jsval *rval);

/**
 * Convert a variant to jsval. Return true on success.
 */
JSBool
xpc_qsVariantToJsval(XPCLazyCallContext &ccx,
                     nsIVariant *p,
                     jsval *rval);

#ifdef DEBUG
void
xpc_qsAssertContextOK(JSContext *cx);

inline bool
xpc_qsSameResult(nsISupports *result1, nsISupports *result2)
{
    return SameCOMIdentity(result1, result2);
}

inline bool
xpc_qsSameResult(const nsString &result1, const nsString &result2)
{
    return result1.Equals(result2);
}

inline bool
xpc_qsSameResult(PRInt32 result1, PRInt32 result2)
{
    return result1 == result2;
}

#define XPC_QS_ASSERT_CONTEXT_OK(cx) xpc_qsAssertContextOK(cx)
#else
#define XPC_QS_ASSERT_CONTEXT_OK(cx) ((void) 0)
#endif

// Apply |op| to |obj|, |id|, and |vp|. If |op| is a setter, treat the assignment as lenient.
template<typename Op>
static inline JSBool ApplyPropertyOp(JSContext *cx, Op op, JSHandleObject obj, JSHandleId id, jsval *vp);

template<>
inline JSBool
ApplyPropertyOp<JSPropertyOp>(JSContext *cx, JSPropertyOp op, JSHandleObject obj, JSHandleId id, jsval *vp)
{
    return op(cx, obj, id, vp);
}

template<>
inline JSBool
ApplyPropertyOp<JSStrictPropertyOp>(JSContext *cx, JSStrictPropertyOp op, JSHandleObject obj,
                                    JSHandleId id, jsval *vp)
{
    return op(cx, obj, id, true, vp);
}

template<typename Op>
JSBool
PropertyOpForwarder(JSContext *cx, unsigned argc, jsval *vp)
{
    // Layout:
    //   this = our this
    //   property op to call = callee reserved slot 0
    //   name of the property = callee reserved slot 1

    JSObject *callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
    JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    jsval v = js::GetFunctionNativeReserved(callee, 0);

    JSObject *ptrobj = JSVAL_TO_OBJECT(v);
    Op *popp = static_cast<Op *>(JS_GetPrivate(ptrobj));

    v = js::GetFunctionNativeReserved(callee, 1);

    jsval argval = (argc > 0) ? JS_ARGV(cx, vp)[0] : JSVAL_VOID;
    JS::RootedId id(cx);
    if (!JS_ValueToId(cx, v, id.address()))
        return false;
    JS_SET_RVAL(cx, vp, argval);
    return ApplyPropertyOp<Op>(cx, *popp, obj, id, vp);
}

extern JSClass PointerHolderClass;

template<typename Op>
JSObject *
GeneratePropertyOp(JSContext *cx, JSObject *obj, jsid id, unsigned argc, Op pop)
{
    // The JS engine provides two reserved slots on function objects for
    // XPConnect to use. Use them to stick the necessary info here.
    JSFunction *fun =
        js::NewFunctionByIdWithReserved(cx, PropertyOpForwarder<Op>, argc, 0, obj, id);
    if (!fun)
        return nsnull;

    JSObject *funobj = JS_GetFunctionObject(fun);

    JS::AutoObjectRooter tvr(cx, funobj);

    // Unfortunately, we cannot guarantee that Op is aligned. Use a
    // second object to work around this.
    JSObject *ptrobj = JS_NewObject(cx, &PointerHolderClass, nsnull, funobj);
    if (!ptrobj)
        return nsnull;
    Op *popp = new Op;
    if (!popp)
        return nsnull;
    *popp = pop;
    JS_SetPrivate(ptrobj, popp);

    js::SetFunctionNativeReserved(funobj, 0, OBJECT_TO_JSVAL(ptrobj));
    js::SetFunctionNativeReserved(funobj, 1, js::IdToJsval(id));
    return funobj;
}

#endif /* xpcquickstubs_h___ */
