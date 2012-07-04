/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/jsipc/ObjectWrapperParent.h"
#include "mozilla/jsipc/ContextWrapperParent.h"
#include "mozilla/jsipc/CPOWTypes.h"
#include "mozilla/unused.h"
#include "nsJSUtils.h"

#include "jsutil.h"
#include "jsfriendapi.h"

using namespace mozilla::jsipc;

namespace {

    // Only need one reserved slot because the ObjectWrapperParent* is
    // stored in the private slot.
    static const unsigned sFlagsSlot = 0;
    static const unsigned sNumSlots = 1;
    static const unsigned CPOW_FLAG_RESOLVING = 1 << 0;

    class AutoResolveFlag
    {
        JSObject* mObj;
        unsigned mOldFlags;
        JS_DECL_USE_GUARD_OBJECT_NOTIFIER

        static unsigned GetFlags(JSObject* obj) {
            jsval v = JS_GetReservedSlot(obj, sFlagsSlot);
            return JSVAL_TO_INT(v);
        }

        static unsigned SetFlags(JSObject* obj, unsigned flags) {
            unsigned oldFlags = GetFlags(obj);
            if (oldFlags != flags)
                JS_SetReservedSlot(obj, sFlagsSlot, INT_TO_JSVAL(flags));
            return oldFlags;
        }

    public:

        AutoResolveFlag(JSObject* obj
                        JS_GUARD_OBJECT_NOTIFIER_PARAM)
            : mObj(obj)
            , mOldFlags(SetFlags(obj, GetFlags(obj) | CPOW_FLAG_RESOLVING))
        {
            JS_GUARD_OBJECT_NOTIFIER_INIT;
        }

        ~AutoResolveFlag() {
            SetFlags(mObj, mOldFlags);
        }

        static JSBool IsSet(JSObject* obj) {
            return GetFlags(obj) & CPOW_FLAG_RESOLVING;
        }

    };

    class StatusMemberOwner
    {
        OperationStatus mStatus;
    public:
        StatusMemberOwner() : mStatus(JS_FALSE) {}
        OperationStatus* StatusPtr() {
            return &mStatus;
        }
    };

    typedef AutoCheckOperationBase<StatusMemberOwner> ACOBase;

    class AutoCheckOperation : public ACOBase
    {
        JS_DECL_USE_GUARD_OBJECT_NOTIFIER
    public:
        AutoCheckOperation(JSContext* cx,
                           ObjectWrapperParent* owp
                           JS_GUARD_OBJECT_NOTIFIER_PARAM)
            : ACOBase(cx, owp)
        {
            JS_GUARD_OBJECT_NOTIFIER_INIT;
        }
    };

}

void
ObjectWrapperParent::CheckOperation(JSContext* cx,
                                    OperationStatus* status)
{
    NS_PRECONDITION(status->type() != OperationStatus::T__None,
                    "Checking an uninitialized operation.");

    switch (status->type()) {
    case OperationStatus::TJSVariant:
        {
            jsval thrown;
            if (jsval_from_JSVariant(cx, status->get_JSVariant(), &thrown))
                JS_SetPendingException(cx, thrown);
            *status = JS_FALSE;
        }
        break;
    case OperationStatus::TJSBool:
        if (!status->get_JSBool() && !JS_IsExceptionPending(cx)) {
            NS_WARNING("CPOW operation failed without setting an exception.");
        }
        break;
    default:
        NS_NOTREACHED("Invalid or uninitialized OperationStatus type.");
        break;
    }
}

template <typename RType>
static RType
with_error(JSContext* cx,
               RType rval,
               const char* error = NULL)
{
    if (!JS_IsExceptionPending(cx))
        JS_ReportError(cx, error ? error : "Unspecified CPOW error");
    return rval;
}

const js::Class ObjectWrapperParent::sCPOW_JSClass = {
      "CrossProcessObjectWrapper",
      JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE |
      JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(sNumSlots),
      ObjectWrapperParent::CPOW_AddProperty,
      ObjectWrapperParent::CPOW_DelProperty,
      ObjectWrapperParent::CPOW_GetProperty,
      ObjectWrapperParent::CPOW_SetProperty,
      (JSEnumerateOp) ObjectWrapperParent::CPOW_NewEnumerate,
      (JSResolveOp) ObjectWrapperParent::CPOW_NewResolve,
      ObjectWrapperParent::CPOW_Convert,
      ObjectWrapperParent::CPOW_Finalize,
      nsnull, // checkAccess
      ObjectWrapperParent::CPOW_Call,
      ObjectWrapperParent::CPOW_HasInstance,
      ObjectWrapperParent::CPOW_Construct,
      nsnull, // trace
      {
          ObjectWrapperParent::CPOW_Equality,
          nsnull, // outerObject
          nsnull, // innerObject
          nsnull, // iteratorObject
          nsnull, // wrappedObject
    }
};

void
ObjectWrapperParent::ActorDestroy(ActorDestroyReason)
{
    if (mObj) {
        JS_SetPrivate(mObj, NULL);
        mObj = NULL;
    }
}

ContextWrapperParent*
ObjectWrapperParent::Manager()
{
    PContextWrapperParent* pcwp = PObjectWrapperParent::Manager();
    return static_cast<ContextWrapperParent*>(pcwp);
}

JSObject*
ObjectWrapperParent::GetJSObject(JSContext* cx) const
{
    if (!mObj) {
        js::Class *clasp = const_cast<js::Class *>(&ObjectWrapperParent::sCPOW_JSClass);
        mObj = JS_NewObject(cx, js::Jsvalify(clasp), NULL, NULL);
        if (mObj) {
            JS_SetPrivate(mObj, (void*)this);
            JS_SetReservedSlot(mObj, sFlagsSlot, JSVAL_ZERO);
        }
    }
    return mObj;
}

static ObjectWrapperParent*
Unwrap(JSObject* obj)
{
    while (js::GetObjectClass(obj) != &ObjectWrapperParent::sCPOW_JSClass)
        if (!(obj = js::GetObjectProto(obj)))
            return NULL;
    
    ObjectWrapperParent* self =
        static_cast<ObjectWrapperParent*>(JS_GetPrivate(obj));

    NS_ASSERTION(!self || self->GetJSObjectOrNull() == obj,
                 "Wrapper and wrapped object disagree?");
    
    return self;
}

/*static*/ bool
ObjectWrapperParent::jsval_to_JSVariant(JSContext* cx, jsval from,
                                        JSVariant* to)
{
    switch (JS_TypeOfValue(cx, from)) {
    case JSTYPE_VOID:
        *to = void_t();
        return true;
    case JSTYPE_NULL:
        if (from != JSVAL_NULL)
            return false;
        // fall through
    case JSTYPE_FUNCTION:
        // CPOWs can fool JS_TypeOfValue into returning JSTYPE_FUNCTION
        // because they have a call hook, but CPOWs are really objects, so
        // fall through to the JSTYPE_OBJECT case:
    case JSTYPE_OBJECT:
        {
            PObjectWrapperParent* powp;
            if (!JSObject_to_PObjectWrapperParent(JSVAL_TO_OBJECT(from), &powp))
                return with_error(cx, false, "Cannot pass parent-created object to child");
            *to = powp;
        }
        return true;
    case JSTYPE_STRING:
        {
            nsDependentJSString depStr;
            if (!depStr.init(cx, from))
                return false;
            *to = depStr;
        }
        return true;
    case JSTYPE_NUMBER:
        if (JSVAL_IS_INT(from))
            *to = JSVAL_TO_INT(from);
        else if (JSVAL_IS_DOUBLE(from))
            *to = JSVAL_TO_DOUBLE(from);
        else return false;
        return true;
    case JSTYPE_BOOLEAN:
        *to = !!JSVAL_TO_BOOLEAN(from);
        return true;
    case JSTYPE_XML:
        return with_error(cx, false, "CPOWs currently cannot handle JSTYPE_XML");
    default:
        return with_error(cx, false, "Bad jsval type");
    }
}

/*static*/ bool
ObjectWrapperParent::jsval_from_JSVariant(JSContext* cx, const JSVariant& from,
                                          jsval* to)
{
    switch (from.type()) {
    case JSVariant::Tvoid_t:
        *to = JSVAL_VOID;
        return true;
    case JSVariant::TPObjectWrapperParent:
        return jsval_from_PObjectWrapperParent(cx, from.get_PObjectWrapperParent(), to);
    case JSVariant::TnsString:
        {
            JSString* str = JS_NewUCStringCopyZ(cx, from.get_nsString().BeginReading());
            if (!str)
                return false;
            *to = STRING_TO_JSVAL(str);
            return true;
        }
    case JSVariant::Tint:
        *to = INT_TO_JSVAL(from.get_int());
        return true;
    case JSVariant::Tdouble:
        return !!JS_NewNumberValue(cx, from.get_double(), to);
    case JSVariant::Tbool:
        *to = BOOLEAN_TO_JSVAL(from.get_bool());
        return true;
    default:
        return false;
    }
}

/*static*/ bool
ObjectWrapperParent::
JSObject_to_PObjectWrapperParent(JSObject* from, PObjectWrapperParent** to)
{
    if (!from) {
        *to = NULL;
        return true;
    }
    ObjectWrapperParent* owp = Unwrap(from);
    if (!owp)
        return false;
    *to = owp;
    return true;
}

/*static*/ bool
ObjectWrapperParent::
JSObject_from_PObjectWrapperParent(JSContext* cx,
                                   const PObjectWrapperParent* from,
                                   JSMutableHandleObject to)
{
    const ObjectWrapperParent* owp =
        static_cast<const ObjectWrapperParent*>(from);
    to.set(owp
           ? owp->GetJSObject(cx)
           : JSVAL_TO_OBJECT(JSVAL_NULL));
    return true;
}

/*static*/ bool
ObjectWrapperParent::
jsval_from_PObjectWrapperParent(JSContext* cx,
                                const PObjectWrapperParent* from,
                                jsval* to)
{
    JS::RootedObject obj(cx);
    if (!JSObject_from_PObjectWrapperParent(cx, from, &obj))
        return false;
    *to = OBJECT_TO_JSVAL(obj);
    return true;
}
    
static bool
jsid_from_int(JSContext* cx, int from, jsid* to)
{
    jsval v = INT_TO_JSVAL(from);
    return JS_ValueToId(cx, v, to);
}

static bool
jsid_from_nsString(JSContext* cx, const nsString& from, jsid* to)
{
    JSString* str = JS_NewUCStringCopyZ(cx, from.BeginReading());
    if (!str)
        return false;
    return JS_ValueToId(cx, STRING_TO_JSVAL(str), to);
}

static bool
jsval_to_nsString(JSContext* cx, jsid from, nsString* to)
{
    JSString* str;
    const jschar* chars;
    jsval idval;
    if (JS_IdToValue(cx, from, &idval) &&
        (str = JS_ValueToString(cx, idval)) &&
        (chars = JS_GetStringCharsZ(cx, str))) {
        *to = chars;
        return true;
    }
    return false;
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_AddProperty(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                      jsval *vp)
{
    CPOW_LOG(("Calling CPOW_AddProperty (%s)...",
              JSVAL_TO_CSTR(cx, id)));

    ObjectWrapperParent* self = Unwrap(obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_AddProperty");

    if (AutoResolveFlag::IsSet(obj))
        return JS_TRUE;

    AutoCheckOperation aco(cx, self);

    nsString in_id;

    if (!jsval_to_nsString(cx, id, &in_id))
        return JS_FALSE;

    return (self->Manager()->RequestRunToCompletion() &&
            self->CallAddProperty(in_id,
                                  aco.StatusPtr()) &&
            aco.Ok());
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_GetProperty(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                      jsval *vp)
{
    CPOW_LOG(("Calling CPOW_GetProperty (%s)...",
              JSVAL_TO_CSTR(cx, id)));

    ObjectWrapperParent* self = Unwrap(obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_GetProperty");

    AutoCheckOperation aco(cx, self);

    nsString in_id;

    if (!jsval_to_nsString(cx, id, &in_id))
        return JS_FALSE;

    JSVariant out_v;
    
    return (self->Manager()->RequestRunToCompletion() &&
            self->CallGetProperty(in_id,
                                  aco.StatusPtr(), &out_v) &&
            aco.Ok() &&
            self->jsval_from_JSVariant(cx, out_v, vp));
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_SetProperty(JSContext *cx, JSHandleObject obj, JSHandleId id, 
                                      JSBool strict, jsval *vp)
{
    CPOW_LOG(("Calling CPOW_SetProperty (%s)...",
              JSVAL_TO_CSTR(cx, id)));

    ObjectWrapperParent* self = Unwrap(obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_SetProperty");

    AutoCheckOperation aco(cx, self);

    nsString in_id;
    JSVariant in_v;

    if (!jsval_to_nsString(cx, id, &in_id) ||
        !self->jsval_to_JSVariant(cx, *vp, &in_v))
        return JS_FALSE;

    JSVariant out_v;

    return (self->Manager()->RequestRunToCompletion() &&
            self->CallSetProperty(in_id, in_v,
                                  aco.StatusPtr(), &out_v) &&
            aco.Ok() &&
            self->jsval_from_JSVariant(cx, out_v, vp));
}    
    
/*static*/ JSBool
ObjectWrapperParent::CPOW_DelProperty(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                      jsval *vp)
{
    CPOW_LOG(("Calling CPOW_DelProperty (%s)...",
              JSVAL_TO_CSTR(cx, id)));

    ObjectWrapperParent* self = Unwrap(obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_DelProperty");

    AutoCheckOperation aco(cx, self);

    nsString in_id;

    if (!jsval_to_nsString(cx, id, &in_id))
        return JS_FALSE;

    JSVariant out_v;
    
    return (self->Manager()->RequestRunToCompletion() &&
            self->CallDelProperty(in_id,
                                  aco.StatusPtr(), &out_v) &&
            aco.Ok() &&
            jsval_from_JSVariant(cx, out_v, vp));
}

JSBool
ObjectWrapperParent::NewEnumerateInit(JSContext* cx, jsval* statep, jsid* idp)
{
    AutoCheckOperation aco(cx, this);

    JSVariant out_state;
    int out_id;

    return (CallNewEnumerateInit(aco.StatusPtr(), &out_state, &out_id) &&
            aco.Ok() &&
            jsval_from_JSVariant(cx, out_state, statep) &&
            (!idp || jsid_from_int(cx, out_id, idp)));
}

JSBool
ObjectWrapperParent::NewEnumerateNext(JSContext* cx, jsval* statep, jsid* idp)
{
    AutoCheckOperation aco(cx, this);

    JSVariant in_state;

    if (!jsval_to_JSVariant(cx, *statep, &in_state))
        return JS_FALSE;

    JSVariant out_state;
    nsString out_id;

    if (CallNewEnumerateNext(in_state,
                             aco.StatusPtr(), &out_state, &out_id) &&
        aco.Ok() &&
        jsval_from_JSVariant(cx, out_state, statep) &&
        jsid_from_nsString(cx, out_id, idp))
    {
        JSObject* obj = GetJSObject(cx);
        AutoResolveFlag arf(obj);
        return JS_DefinePropertyById(cx, obj, *idp, JSVAL_VOID, NULL, NULL,
                                     JSPROP_ENUMERATE);
    }
    return JS_FALSE;
}

JSBool
ObjectWrapperParent::NewEnumerateDestroy(JSContext* cx, jsval state)
{
    AutoCheckOperation aco(cx, this);

    JSVariant in_state;

    if (!jsval_to_JSVariant(cx, state, &in_state))
        return JS_FALSE;

    return SendNewEnumerateDestroy(in_state);
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_NewEnumerate(JSContext *cx, JSHandleObject obj,
                                       JSIterateOp enum_op, jsval *statep,
                                       jsid *idp)
{
    CPOW_LOG(("Calling CPOW_NewEnumerate..."));

    ObjectWrapperParent* self = Unwrap(obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_NewEnumerate");

    switch (enum_op) {
    case JSENUMERATE_INIT:
    case JSENUMERATE_INIT_ALL:
        self->Manager()->RequestRunToCompletion();
        return self->NewEnumerateInit(cx, statep, idp);
    case JSENUMERATE_NEXT:
        return self->NewEnumerateNext(cx, statep, idp);
    case JSENUMERATE_DESTROY:
        return self->NewEnumerateDestroy(cx, *statep);
    }

    NS_NOTREACHED("Unknown enum_op value in CPOW_NewEnumerate");
    return JS_FALSE;
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_NewResolve(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                     unsigned flags, JSMutableHandleObject objp)
{
    CPOW_LOG(("Calling CPOW_NewResolve (%s)...",
              JSVAL_TO_CSTR(cx, id)));

    ObjectWrapperParent* self = Unwrap(obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_NewResolve");

    AutoCheckOperation aco(cx, self);

    nsString in_id;

    if (!jsval_to_nsString(cx, id, &in_id))
        return JS_FALSE;

    PObjectWrapperParent* out_pobj;

    if (!self->Manager()->RequestRunToCompletion() ||
        !self->CallNewResolve(in_id, flags,
                              aco.StatusPtr(), &out_pobj) ||
        !aco.Ok() ||
        !JSObject_from_PObjectWrapperParent(cx, out_pobj, objp))
        return JS_FALSE;

    if (objp) {
        AutoResolveFlag arf(objp);
        JS::RootedObject obj2(cx, objp);
        JS_DefinePropertyById(cx, obj2, id, JSVAL_VOID, NULL, NULL,
                              JSPROP_ENUMERATE);
    }
    return JS_TRUE;
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_Convert(JSContext *cx, JSHandleObject obj, JSType type,
                                  jsval *vp)
{
    CPOW_LOG(("Calling CPOW_Convert (to %s)...",
              JS_GetTypeName(cx, type)));

    ObjectWrapperParent* self = Unwrap(obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_Convert");

    *vp = OBJECT_TO_JSVAL(obj);

    return JS_TRUE;
}

/*static*/ void
ObjectWrapperParent::CPOW_Finalize(js::FreeOp* fop, JSObject* obj)
{
    CPOW_LOG(("Calling CPOW_Finalize..."));
    
    ObjectWrapperParent* self = Unwrap(obj);
    if (self) {
        self->mObj = NULL;
        unused << ObjectWrapperParent::Send__delete__(self);
    }
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_Call(JSContext* cx, unsigned argc, jsval* vp)
{
    CPOW_LOG(("Calling CPOW_Call..."));

    JSObject* thisobj = JS_THIS_OBJECT(cx, vp);
    if (!thisobj)
        return JS_FALSE;

    ObjectWrapperParent* function =
        Unwrap(JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));
    if (!function)
        return with_error(cx, JS_FALSE, "Could not unwrap CPOW function");

    AutoCheckOperation aco(cx, function);

    ObjectWrapperParent* receiver = Unwrap(thisobj);
    if (!receiver) {
        // Substitute child global for parent global object.
        // TODO First make sure we're really replacing the global object?
        ContextWrapperParent* manager =
            static_cast<ContextWrapperParent*>(function->Manager());
        receiver = manager->GetGlobalObjectWrapper();
    }

    InfallibleTArray<JSVariant> in_argv(argc);
    jsval* argv = JS_ARGV(cx, vp);
    for (unsigned i = 0; i < argc; i++)
        if (!jsval_to_JSVariant(cx, argv[i], in_argv.AppendElement()))
            return JS_FALSE;

    JSVariant out_rval;

    return (function->Manager()->RequestRunToCompletion() &&
            function->CallCall(receiver, in_argv,
                               aco.StatusPtr(), &out_rval) &&
            aco.Ok() &&
            jsval_from_JSVariant(cx, out_rval, vp));
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_Construct(JSContext* cx, unsigned argc, jsval* vp)
{
    CPOW_LOG(("Calling CPOW_Construct..."));
    
    ObjectWrapperParent* constructor = Unwrap(JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));
    if (!constructor)
        return with_error(cx, JS_FALSE, "Could not unwrap CPOW constructor function");

    AutoCheckOperation aco(cx, constructor);

    InfallibleTArray<JSVariant> in_argv(argc);
    jsval* argv = JS_ARGV(cx, vp);
    for (unsigned i = 0; i < argc; i++)
        if (!jsval_to_JSVariant(cx, argv[i], in_argv.AppendElement()))
            return JS_FALSE;

    PObjectWrapperParent* out_powp;

    return (constructor->Manager()->RequestRunToCompletion() &&
            constructor->CallConstruct(in_argv, aco.StatusPtr(), &out_powp) &&
            aco.Ok() &&
            jsval_from_PObjectWrapperParent(cx, out_powp, vp));
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_HasInstance(JSContext *cx, JSHandleObject obj, const jsval *v,
                                      JSBool *bp)
{
    CPOW_LOG(("Calling CPOW_HasInstance..."));

    *bp = JS_FALSE;

    ObjectWrapperParent* self = Unwrap(obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_HasInstance");

    AutoCheckOperation aco(cx, self);

    JSVariant in_v;

    if (!jsval_to_JSVariant(cx, *v, &in_v))
        return JS_FALSE;

    return (self->Manager()->RequestRunToCompletion() &&
            self->CallHasInstance(in_v,
                                  aco.StatusPtr(), bp) &&
            aco.Ok());
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_Equality(JSContext *cx, JSHandleObject obj, const jsval *v,
                                   JSBool *bp)
{
    CPOW_LOG(("Calling CPOW_Equality..."));

    *bp = JS_FALSE;
    
    ObjectWrapperParent* self = Unwrap(obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_Equality");

    if (JSVAL_IS_PRIMITIVE(*v))
        return JS_TRUE;

    ObjectWrapperParent* other = Unwrap(JSVAL_TO_OBJECT(*v));
    if (!other)
        return JS_TRUE;

    *bp = (self == other);
    
    return JS_TRUE;
}
