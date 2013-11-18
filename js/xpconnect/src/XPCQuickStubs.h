/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcquickstubs_h___
#define xpcquickstubs_h___

#include "XPCForwards.h"

class qsObjectHelper;
namespace mozilla {
namespace dom {
class NativeProperties;
}
}

/* XPCQuickStubs.h - Support functions used only by quick stubs. */

class XPCCallContext;

#define XPC_QS_NULL_INDEX  ((uint16_t) -1)

struct xpc_qsPropertySpec {
    uint16_t name_index;
    JSNative getter;
    JSNative setter;
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
    const mozilla::dom::NativeProperties* newBindingProperties;
    // These last two fields index to other entries in the same table.
    // XPC_QS_NULL_ENTRY indicates there are no more entries in the chain.
    uint16_t parentInterface;
    uint16_t chain;
};

bool
xpc_qsDefineQuickStubs(JSContext *cx, JSObject *proto, unsigned extraFlags,
                       uint32_t ifacec, const nsIID **interfaces,
                       uint32_t tableSize, const xpc_qsHashEntry *table,
                       const xpc_qsPropertySpec *propspecs,
                       const xpc_qsFunctionSpec *funcspecs,
                       const char *stringTable);

/** Raise an exception on @a cx and return false. */
bool
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
bool
xpc_qsThrowGetterSetterFailed(JSContext *cx, nsresult rv,
                              JSObject *obj, jsid memberId);
// And variants using strings and string tables
bool
xpc_qsThrowGetterSetterFailed(JSContext *cx, nsresult rv,
                              JSObject *obj, const char* memberName);
bool
xpc_qsThrowGetterSetterFailed(JSContext *cx, nsresult rv,
                              JSObject *obj, uint16_t memberIndex);

/**
 * Fail after an XPCOM method returned rv.
 *
 * See NOTE at xpc_qsThrowGetterSetterFailed.
 */
bool
xpc_qsThrowMethodFailed(JSContext *cx, nsresult rv, jsval *vp);

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
// And variants using strings and string tables
void
xpc_qsThrowBadSetterValue(JSContext *cx, nsresult rv, JSObject *obj,
                          const char* propName);
void
xpc_qsThrowBadSetterValue(JSContext *cx, nsresult rv, JSObject *obj,
                          uint16_t name_index);


bool
xpc_qsGetterOnlyPropertyStub(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                             bool strict, JS::MutableHandleValue vp);

bool
xpc_qsGetterOnlyNativeStub(JSContext *cx, unsigned argc, jsval *vp);

/* Functions for converting values between COM and JS. */

inline bool
xpc_qsInt64ToJsval(JSContext *cx, int64_t i, JS::MutableHandleValue rv)
{
    rv.setNumber(static_cast<double>(i));
    return true;
}

inline bool
xpc_qsUint64ToJsval(JSContext *cx, uint64_t u, JS::MutableHandleValue rv)
{
    rv.setNumber(static_cast<double>(u));
    return true;
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

    bool IsValid() const { return mValid; }

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
    bool mValid;

    /*
     * If null is returned, then we either failed or fully initialized
     * |this|; in either case the caller should return immediately
     * without doing anything else. Otherwise, the JSString* created
     * from |v| will be returned.  It'll be rooted, as needed, in
     * *pval.  nullBehavior and undefinedBehavior control what happens
     * when |v| is JSVAL_IS_NULL and JSVAL_IS_VOID respectively.
     */
    template<class traits>
    JSString* InitOrStringify(JSContext* cx, JS::HandleValue v,
                              JS::MutableHandleValue pval,
                              bool notpassed,
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
            if (behavior != eStringify || notpassed) {
                // Here behavior == eStringify implies notpassed, so both eNull and
                // eStringify should end up with void strings.
                (new(mBuf) implementation_type(traits::sEmptyBuffer, uint32_t(0)))->
                    SetIsVoid(behavior != eEmpty);
                mValid = true;
                return nullptr;
            }

            s = JS::ToString(cx, v);
            if (!s) {
                mValid = false;
                return nullptr;
            }
            pval.setString(s);  // Root the new string.
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
    xpc_qsDOMString(JSContext *cx, JS::HandleValue v,
                    JS::MutableHandleValue pval, bool notpassed,
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
    xpc_qsAString(JSContext *cx, JS::HandleValue v,
                  JS::MutableHandleValue pval, bool notpassed)
        : xpc_qsDOMString(cx, v, pval, notpassed, eNull, eNull)
    {}
};

/**
 * Like xpc_qsDOMString and xpc_qsAString, but for XPIDL native types annotated
 * with [cstring] rather than [domstring] or [astring].
 */
class xpc_qsACString : public xpc_qsBasicString<nsACString, nsCString>
{
public:
    xpc_qsACString(JSContext *cx, JS::HandleValue v,
                   JS::MutableHandleValue pval, bool notpassed,
                   StringificationBehavior nullBehavior,
                   StringificationBehavior undefinedBehavior);
};

/**
 * And similar for AUTF8String.
 */
class xpc_qsAUTF8String :
  public xpc_qsBasicString<nsACString, NS_ConvertUTF16toUTF8>
{
public:
  xpc_qsAUTF8String(JSContext* cx, JS::HandleValue v,
                    JS::MutableHandleValue pval, bool notpassed);
};

struct xpc_qsSelfRef
{
    xpc_qsSelfRef() : ptr(nullptr) {}
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
bool
xpc_qsJsvalToCharStr(JSContext *cx, jsval v, JSAutoByteString *bytes);

bool
xpc_qsJsvalToWcharStr(JSContext *cx, jsval v, JS::MutableHandleValue pval, const PRUnichar **pstr);


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
           JS::MutableHandleValue vp);

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
inline bool
xpc_qsUnwrapThis(JSContext *cx,
                 JS::HandleObject obj,
                 T **ppThis,
                 nsISupports **pThisRef,
                 JS::MutableHandleValue pThisVal,
                 bool failureFatal = true)
{
    XPCWrappedNative *wrapper;
    XPCWrappedNativeTearOff *tearoff;
    JS::RootedObject current(cx);
    nsresult rv = getWrapper(cx, obj, &wrapper, current.address(), &tearoff);
    if (NS_SUCCEEDED(rv))
        rv = castNative(cx, wrapper, current, tearoff, NS_GET_TEMPLATE_IID(T),
                        reinterpret_cast<void **>(ppThis), pThisRef, pThisVal);

    if (failureFatal)
        return NS_SUCCEEDED(rv) || xpc_qsThrow(cx, rv);

    if (NS_FAILED(rv))
        *ppThis = nullptr;
    return true;
}

nsISupports*
castNativeFromWrapper(JSContext *cx,
                      JSObject *obj,
                      uint32_t interfaceBit,
                      uint32_t protoID,
                      int32_t protoDepth,
                      nsISupports **pRef,
                      JS::MutableHandleValue pVal,
                      nsresult *rv);

bool
xpc_qsUnwrapThisFromCcxImpl(XPCCallContext &ccx,
                            const nsIID &iid,
                            void **ppThis,
                            nsISupports **pThisRef,
                            JS::MutableHandleValue vp);

/**
 * Alternate implementation of xpc_qsUnwrapThis using information already
 * present in the given XPCCallContext.
 */
template <class T>
inline bool
xpc_qsUnwrapThisFromCcx(XPCCallContext &ccx,
                        T **ppThis,
                        nsISupports **pThisRef,
                        JS::MutableHandleValue pThisVal)
{
    return xpc_qsUnwrapThisFromCcxImpl(ccx,
                                       NS_GET_TEMPLATE_IID(T),
                                       reinterpret_cast<void **>(ppThis),
                                       pThisRef,
                                       pThisVal);
}

MOZ_ALWAYS_INLINE JSObject*
xpc_qsUnwrapObj(jsval v, nsISupports **ppArgRef, nsresult *rv)
{
    *rv = NS_OK;
    if (v.isObject()) {
        return &v.toObject();
    }

    if (!v.isNullOrUndefined()) {
        *rv = ((v.isInt32() && v.toInt32() == 0)
               ? NS_ERROR_XPC_BAD_CONVERT_JS_ZERO_ISNOT_NULL
               : NS_ERROR_XPC_BAD_CONVERT_JS);
    }

    *ppArgRef = nullptr;
    return nullptr;
}

nsresult
xpc_qsUnwrapArgImpl(JSContext *cx, JS::HandleValue v, const nsIID &iid, void **ppArg,
                    nsISupports **ppArgRef, JS::MutableHandleValue vp);

/** Convert a jsval to an XPCOM pointer. */
template <class Interface, class StrongRefType>
inline nsresult
xpc_qsUnwrapArg(JSContext *cx, JS::HandleValue v, Interface **ppArg,
                StrongRefType **ppArgRef, JS::MutableHandleValue vp)
{
    nsISupports* argRef = *ppArgRef;
    nsresult rv = xpc_qsUnwrapArgImpl(cx, v, NS_GET_TEMPLATE_IID(Interface),
                                      reinterpret_cast<void **>(ppArg), &argRef,
                                      vp);
    *ppArgRef = static_cast<StrongRefType*>(argRef);
    return rv;
}

MOZ_ALWAYS_INLINE nsISupports*
castNativeArgFromWrapper(JSContext *cx,
                         jsval v,
                         uint32_t bit,
                         uint32_t protoID,
                         int32_t protoDepth,
                         nsISupports **pArgRef,
                         JS::MutableHandleValue vp,
                         nsresult *rv)
{
    JSObject *src = xpc_qsUnwrapObj(v, pArgRef, rv);
    if (!src)
        return nullptr;

    return castNativeFromWrapper(cx, src, bit, protoID, protoDepth, pArgRef, vp, rv);
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
    return nullptr;
}

/** Convert an XPCOM pointer to jsval. Return true on success.
 * aIdentity is a performance optimization. Set it to true,
 * only if p is the identity pointer.
 */
bool
xpc_qsXPCOMObjectToJsval(JSContext *aCx,
                         qsObjectHelper &aHelper,
                         const nsIID *iid,
                         XPCNativeInterface **iface,
                         JS::MutableHandleValue rval);

/**
 * Convert a variant to jsval. Return true on success.
 */
bool
xpc_qsVariantToJsval(JSContext *cx,
                     nsIVariant *p,
                     JS::MutableHandleValue rval);

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
xpc_qsSameResult(int32_t result1, int32_t result2)
{
    return result1 == result2;
}

#define XPC_QS_ASSERT_CONTEXT_OK(cx) xpc_qsAssertContextOK(cx)
#else
#define XPC_QS_ASSERT_CONTEXT_OK(cx) ((void) 0)
#endif

// Apply |op| to |obj|, |id|, and |vp|. If |op| is a setter, treat the assignment as lenient.
template<typename Op>
inline bool ApplyPropertyOp(JSContext *cx, Op op, JS::HandleObject obj, JS::HandleId id,
                              JS::MutableHandleValue vp);

template<>
inline bool
ApplyPropertyOp<JSPropertyOp>(JSContext *cx, JSPropertyOp op, JS::HandleObject obj, JS::HandleId id,
                              JS::MutableHandleValue vp)
{
    return op(cx, obj, id, vp);
}

template<>
inline bool
ApplyPropertyOp<JSStrictPropertyOp>(JSContext *cx, JSStrictPropertyOp op, JS::HandleObject obj,
                                    JS::HandleId id, JS::MutableHandleValue vp)
{
    return op(cx, obj, id, true, vp);
}

template<typename Op>
bool
PropertyOpForwarder(JSContext *cx, unsigned argc, jsval *vp)
{
    // Layout:
    //   this = our this
    //   property op to call = callee reserved slot 0
    //   name of the property = callee reserved slot 1

    JS::CallArgs args = CallArgsFromVp(argc, vp);

    JS::RootedObject callee(cx, &args.callee());
    JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    jsval v = js::GetFunctionNativeReserved(callee, 0);

    JSObject *ptrobj = JSVAL_TO_OBJECT(v);
    Op *popp = static_cast<Op *>(JS_GetPrivate(ptrobj));

    v = js::GetFunctionNativeReserved(callee, 1);

    JS::RootedValue argval(cx, (argc > 0) ? args.get(0) : JSVAL_VOID);
    JS::RootedId id(cx);
    if (!JS_ValueToId(cx, v, id.address()))
        return false;
    args.rval().set(argval);
    return ApplyPropertyOp<Op>(cx, *popp, obj, id, args.rval());
}

extern const JSClass PointerHolderClass;

template<typename Op>
JSObject *
GeneratePropertyOp(JSContext *cx, JS::HandleObject obj, JS::HandleId id, unsigned argc, Op pop)
{
    // The JS engine provides two reserved slots on function objects for
    // XPConnect to use. Use them to stick the necessary info here.
    JSFunction *fun =
        js::NewFunctionByIdWithReserved(cx, PropertyOpForwarder<Op>, argc, 0, obj, id);
    if (!fun)
        return nullptr;

    JS::RootedObject funobj(cx, JS_GetFunctionObject(fun));

    // Unfortunately, we cannot guarantee that Op is aligned. Use a
    // second object to work around this.
    JSObject *ptrobj = JS_NewObject(cx, &PointerHolderClass, nullptr, funobj);
    if (!ptrobj)
        return nullptr;
    Op *popp = new Op;
    if (!popp)
        return nullptr;
    *popp = pop;
    JS_SetPrivate(ptrobj, popp);

    js::SetFunctionNativeReserved(funobj, 0, OBJECT_TO_JSVAL(ptrobj));
    js::SetFunctionNativeReserved(funobj, 1, js::IdToJsval(id));
    return funobj;
}

#endif /* xpcquickstubs_h___ */
