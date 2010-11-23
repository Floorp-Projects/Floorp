/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
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
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Newman <b{enjam,newma}n@mozilla.com> (original author)
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

#include "mozilla/jsipc/ObjectWrapperParent.h"
#include "mozilla/jsipc/ContextWrapperParent.h"
#include "mozilla/jsipc/CPOWTypes.h"
#include "mozilla/unused.h"

#include "jsobj.h"
#include "jsfun.h"
#include "jsutil.h"

using namespace mozilla::jsipc;

namespace {

    // Only need one reserved slot because the ObjectWrapperParent* is
    // stored in the private slot.
    static const uintN sFlagsSlot = 0;
    static const uintN sNumSlots = 1;
    static const uintN CPOW_FLAG_RESOLVING = 1 << 0;

    class AutoResolveFlag
    {
        JSContext* mContext;
        JSObject* mObj;
        uintN mOldFlags;
        JS_DECL_USE_GUARD_OBJECT_NOTIFIER;

        static uintN GetFlags(JSContext* cx, JSObject* obj) {
            jsval v;
#ifdef DEBUG
            JSBool ok =
#endif
                JS_GetReservedSlot(cx, obj, sFlagsSlot, &v);
            NS_ASSERTION(ok, "Failed to get CPOW flags");
            return JSVAL_TO_INT(v);
        }

        static uintN SetFlags(JSContext* cx, JSObject* obj, uintN flags) {
            uintN oldFlags = GetFlags(cx, obj);
            if (oldFlags != flags)
                JS_SetReservedSlot(cx, obj, sFlagsSlot, INT_TO_JSVAL(flags));
            return oldFlags;
        }

    public:

        AutoResolveFlag(JSContext* cx,
                        JSObject* obj
                        JS_GUARD_OBJECT_NOTIFIER_PARAM)
            : mContext(cx)
            , mObj(obj)
            , mOldFlags(SetFlags(cx, obj,
                                 GetFlags(cx, obj) | CPOW_FLAG_RESOLVING))
        {
            JS_GUARD_OBJECT_NOTIFIER_INIT;
        }

        ~AutoResolveFlag() {
            SetFlags(mContext, mObj, mOldFlags);
        }

        static JSBool IsSet(JSContext* cx, JSObject* obj) {
            return GetFlags(cx, obj) & CPOW_FLAG_RESOLVING;
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
        JS_DECL_USE_GUARD_OBJECT_NOTIFIER;
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
      JS_VALUEIFY(js::PropertyOp, ObjectWrapperParent::CPOW_AddProperty),
      JS_VALUEIFY(js::PropertyOp, ObjectWrapperParent::CPOW_DelProperty),
      JS_VALUEIFY(js::PropertyOp, ObjectWrapperParent::CPOW_GetProperty),
      JS_VALUEIFY(js::PropertyOp, ObjectWrapperParent::CPOW_SetProperty),
      (JSEnumerateOp) ObjectWrapperParent::CPOW_NewEnumerate,
      (JSResolveOp) ObjectWrapperParent::CPOW_NewResolve,
      JS_VALUEIFY(js::ConvertOp, ObjectWrapperParent::CPOW_Convert),
      ObjectWrapperParent::CPOW_Finalize,
      nsnull, // reserved1
      nsnull, // checkAccess
      JS_VALUEIFY(js::CallOp, ObjectWrapperParent::CPOW_Call),
      JS_VALUEIFY(js::CallOp, ObjectWrapperParent::CPOW_Construct),
      nsnull, // xdrObject
      JS_VALUEIFY(js::HasInstanceOp, ObjectWrapperParent::CPOW_HasInstance),
      nsnull, // mark
      {
          JS_VALUEIFY(js::EqualityOp, ObjectWrapperParent::CPOW_Equality),
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
        mObj->setPrivate(NULL);
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
    js::Class *clasp = const_cast<js::Class *>(&ObjectWrapperParent::sCPOW_JSClass);
    if (!mObj && (mObj = JS_NewObject(cx, js::Jsvalify(clasp), NULL, NULL))) {
        JS_SetPrivate(cx, mObj, (void*)this);
        JS_SetReservedSlot(cx, mObj, sFlagsSlot, JSVAL_ZERO);
    }
    return mObj;
}

static ObjectWrapperParent*
Unwrap(JSContext* cx, JSObject* obj)
{
    while (obj->getClass() != &ObjectWrapperParent::sCPOW_JSClass)
        if (!(obj = obj->getProto()))
            return NULL;
    
    ObjectWrapperParent* self =
        static_cast<ObjectWrapperParent*>(JS_GetPrivate(cx, obj));

    NS_ASSERTION(!self || self->GetJSObject(cx) == obj,
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
            if (!JSObject_to_PObjectWrapperParent(cx, JSVAL_TO_OBJECT(from), &powp))
                return with_error(cx, false, "Cannot pass parent-created object to child");
            *to = powp;
        }
        return true;
    case JSTYPE_STRING:
        *to = nsDependentString((PRUnichar*)JS_GetStringChars(JSVAL_TO_STRING(from)),
                                JS_GetStringLength(JSVAL_TO_STRING(from)));
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
JSObject_to_PObjectWrapperParent(JSContext* cx, JSObject* from,
                                 PObjectWrapperParent** to)
{
    if (!from) {
        *to = NULL;
        return true;
    }
    ObjectWrapperParent* owp = Unwrap(cx, from);
    if (!owp)
        return false;
    *to = owp;
    return true;
}

/*static*/ bool
ObjectWrapperParent::
JSObject_from_PObjectWrapperParent(JSContext* cx,
                                   const PObjectWrapperParent* from,
                                   JSObject** to)
{
    const ObjectWrapperParent* owp =
        static_cast<const ObjectWrapperParent*>(from);
    *to = owp
        ? owp->GetJSObject(cx)
        : JSVAL_TO_OBJECT(JSVAL_NULL);
    return true;
}

/*static*/ bool
ObjectWrapperParent::
jsval_from_PObjectWrapperParent(JSContext* cx,
                                const PObjectWrapperParent* from,
                                jsval* to)
{
    JSObject* obj;
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
    jsval idval;
    if (JS_IdToValue(cx, from, &idval) &&
        (str = JS_ValueToString(cx, idval))) {
        *to = JS_GetStringChars(str);
        return true;
    }
    return false;
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_AddProperty(JSContext *cx, JSObject *obj, jsid id,
                                      jsval *vp)
{
    CPOW_LOG(("Calling CPOW_AddProperty (%s)...",
              JSVAL_TO_CSTR(cx, id)));

    ObjectWrapperParent* self = Unwrap(cx, obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_AddProperty");

    if (AutoResolveFlag::IsSet(cx, obj))
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
ObjectWrapperParent::CPOW_GetProperty(JSContext *cx, JSObject *obj, jsid id,
                                      jsval *vp)
{
    CPOW_LOG(("Calling CPOW_GetProperty (%s)...",
              JSVAL_TO_CSTR(cx, id)));

    ObjectWrapperParent* self = Unwrap(cx, obj);
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
ObjectWrapperParent::CPOW_SetProperty(JSContext *cx, JSObject *obj, jsid id,
                                      jsval *vp)
{
    CPOW_LOG(("Calling CPOW_SetProperty (%s)...",
              JSVAL_TO_CSTR(cx, id)));

    ObjectWrapperParent* self = Unwrap(cx, obj);
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
ObjectWrapperParent::CPOW_DelProperty(JSContext *cx, JSObject *obj, jsid id,
                                      jsval *vp)
{
    CPOW_LOG(("Calling CPOW_DelProperty (%s)...",
              JSVAL_TO_CSTR(cx, id)));

    ObjectWrapperParent* self = Unwrap(cx, obj);
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
        AutoResolveFlag arf(cx, obj);
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
ObjectWrapperParent::CPOW_NewEnumerate(JSContext *cx, JSObject *obj,
                                       JSIterateOp enum_op, jsval *statep,
                                       jsid *idp)
{
    CPOW_LOG(("Calling CPOW_NewEnumerate..."));

    ObjectWrapperParent* self = Unwrap(cx, obj);
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
ObjectWrapperParent::CPOW_NewResolve(JSContext *cx, JSObject *obj, jsid id,
                                     uintN flags, JSObject **objp)
{
    CPOW_LOG(("Calling CPOW_NewResolve (%s)...",
              JSVAL_TO_CSTR(cx, id)));

    ObjectWrapperParent* self = Unwrap(cx, obj);
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

    if (*objp) {
        AutoResolveFlag arf(cx, *objp);
        JS_DefinePropertyById(cx, *objp, id, JSVAL_VOID, NULL, NULL,
                              JSPROP_ENUMERATE);
    }
    return JS_TRUE;
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_Convert(JSContext *cx, JSObject *obj, JSType type,
                                  jsval *vp)
{
    CPOW_LOG(("Calling CPOW_Convert (to %s)...",
              JS_GetTypeName(cx, type)));

    ObjectWrapperParent* self = Unwrap(cx, obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_Convert");

    *vp = OBJECT_TO_JSVAL(obj);

    return JS_TRUE;
}

/*static*/ void
ObjectWrapperParent::CPOW_Finalize(JSContext* cx, JSObject* obj)
{
    CPOW_LOG(("Calling CPOW_Finalize..."));
    
    ObjectWrapperParent* self = Unwrap(cx, obj);
    if (self) {
        self->mObj = NULL;
        unused << ObjectWrapperParent::Send__delete__(self);
    }
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_Call(JSContext* cx, uintN argc, jsval* vp)
{
    CPOW_LOG(("Calling CPOW_Call..."));

    JSObject* thisobj = JS_THIS_OBJECT(cx, vp);
    if (!thisobj)
        return JS_FALSE;

    ObjectWrapperParent* function =
        Unwrap(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));
    if (!function)
        return with_error(cx, JS_FALSE, "Could not unwrap CPOW function");

    AutoCheckOperation aco(cx, function);

    ObjectWrapperParent* receiver = Unwrap(cx, thisobj);
    if (!receiver) {
        // Substitute child global for parent global object.
        // TODO First make sure we're really replacing the global object?
        ContextWrapperParent* manager =
            static_cast<ContextWrapperParent*>(function->Manager());
        receiver = manager->GetGlobalObjectWrapper();
    }

    InfallibleTArray<JSVariant> in_argv(argc);
    jsval* argv = JS_ARGV(cx, vp);
    for (uintN i = 0; i < argc; i++)
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
ObjectWrapperParent::CPOW_Construct(JSContext* cx, uintN argc, jsval* vp)
{
    CPOW_LOG(("Calling CPOW_Construct..."));
    
    ObjectWrapperParent* constructor = Unwrap(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));
    if (!constructor)
        return with_error(cx, JS_FALSE, "Could not unwrap CPOW constructor function");

    AutoCheckOperation aco(cx, constructor);

    InfallibleTArray<JSVariant> in_argv(argc);
    jsval* argv = JS_ARGV(cx, vp);
    for (uintN i = 0; i < argc; i++)
        if (!jsval_to_JSVariant(cx, argv[i], in_argv.AppendElement()))
            return JS_FALSE;

    PObjectWrapperParent* out_powp;

    return (constructor->Manager()->RequestRunToCompletion() &&
            constructor->CallConstruct(in_argv, aco.StatusPtr(), &out_powp) &&
            aco.Ok() &&
            jsval_from_PObjectWrapperParent(cx, out_powp, vp));
}

/*static*/ JSBool
ObjectWrapperParent::CPOW_HasInstance(JSContext *cx, JSObject *obj, const jsval *v,
                                      JSBool *bp)
{
    CPOW_LOG(("Calling CPOW_HasInstance..."));

    *bp = JS_FALSE;

    ObjectWrapperParent* self = Unwrap(cx, obj);
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
ObjectWrapperParent::CPOW_Equality(JSContext *cx, JSObject *obj, const jsval *v,
                                   JSBool *bp)
{
    CPOW_LOG(("Calling CPOW_Equality..."));

    *bp = JS_FALSE;
    
    ObjectWrapperParent* self = Unwrap(cx, obj);
    if (!self)
        return with_error(cx, JS_FALSE, "Unwrapping failed in CPOW_Equality");

    if (JSVAL_IS_PRIMITIVE(*v))
        return JS_TRUE;

    ObjectWrapperParent* other = Unwrap(cx, JSVAL_TO_OBJECT(*v));
    if (!other)
        return JS_TRUE;

    *bp = (self == other);
    
    return JS_TRUE;
}
