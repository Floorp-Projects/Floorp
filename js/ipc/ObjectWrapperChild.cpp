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

#include "base/basictypes.h"

#include "mozilla/jsipc/ContextWrapperChild.h"
#include "mozilla/jsipc/ObjectWrapperChild.h"
#include "mozilla/jsipc/CPOWTypes.h"

#include "jsapi.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsContentUtils.h"
#include "nsIJSContextStack.h"
#include "nsJSUtils.h"

using namespace mozilla::jsipc;
using namespace js;

namespace {

    class AutoContextPusher {

        nsCxPusher mStack;
        JSAutoRequest mRequest;
        JSContext* const mContext;
        const uint32 mSavedOptions;
        JS_DECL_USE_GUARD_OBJECT_NOTIFIER

    public:

        AutoContextPusher(JSContext* cx
                          JS_GUARD_OBJECT_NOTIFIER_PARAM)
            : mRequest(cx)
            , mContext(cx)
            , mSavedOptions(JS_SetOptions(cx, (JS_GetOptions(cx) |
                                               JSOPTION_DONT_REPORT_UNCAUGHT)))
        {
            JS_GUARD_OBJECT_NOTIFIER_INIT;
            mStack.Push(cx, false);
        }

        ~AutoContextPusher() {
            mStack.Pop();
            JS_SetOptions(mContext, mSavedOptions);
        }

    };

    class StatusPtrOwner
    {
        OperationStatus* mStatusPtr;
    public:
        StatusPtrOwner() : mStatusPtr(NULL) {}
        void SetStatusPtr(OperationStatus* statusPtr) {
            mStatusPtr = statusPtr;
            // By default, initialize mStatusPtr to failure without an
            // exception.  Doing so only when the union is uninitialized
            // allows AutoCheckOperation classes to be nested on the
            // stack, just in case AnswerConstruct, for example, calls
            // AnswerCall (as it once did, before there were unrelated
            // problems with that approach).
            if (mStatusPtr->type() == OperationStatus::T__None)
                *mStatusPtr = JS_FALSE;
        }
        OperationStatus* StatusPtr() {
            NS_ASSERTION(mStatusPtr, "Should have called SetStatusPtr by now.");
            return mStatusPtr;
        }
    };

    typedef AutoCheckOperationBase<StatusPtrOwner> ACOBase;

    class AutoCheckOperation : public ACOBase
    {
        JS_DECL_USE_GUARD_OBJECT_NOTIFIER
    public:
        AutoCheckOperation(ObjectWrapperChild* owc,
                           OperationStatus* statusPtr
                           JS_GUARD_OBJECT_NOTIFIER_PARAM)
            : ACOBase(NULL, owc)
        {
            JS_GUARD_OBJECT_NOTIFIER_INIT;
            SetStatusPtr(statusPtr);
        }
    };
}

void
ObjectWrapperChild::CheckOperation(JSContext*,
                                   OperationStatus* status)
{
    NS_PRECONDITION(status->type() != OperationStatus::T__None,
                    "Checking an uninitialized operation.");

    JSContext* cx = Manager()->GetContext();
    jsval thrown;

    if (JS_GetPendingException(cx, &thrown)) {
        NS_ASSERTION(!(status->type() == OperationStatus::TJSBool &&
                       status->get_JSBool()),
                     "Operation succeeded but exception was thrown?");
        JSVariant exception;
        if (!jsval_to_JSVariant(cx, thrown, &exception))
            exception = void_t(); // XXX Useful?
        *status = exception;
        JS_ClearPendingException(cx);
    }
}

ObjectWrapperChild::ObjectWrapperChild(JSContext* cx, JSObject* obj)
    : mObj(obj)
{
    JSAutoRequest request(cx);
#ifdef DEBUG
    bool added =
#endif
         JS_AddObjectRoot(cx, &mObj);
    NS_ASSERTION(added, "ObjectWrapperChild constructor failed to root JSObject*");
}

void
ObjectWrapperChild::ActorDestroy(ActorDestroyReason why)
{
    JSContext* cx = Manager()->GetContext();
    JSAutoRequest request(cx);
    JS_RemoveObjectRoot(cx, &mObj);
}

bool
ObjectWrapperChild::JSObject_to_JSVariant(JSContext* cx, JSObject* from,
                                          JSVariant* to)
{
    *to = Manager()->GetOrCreateWrapper(from);
    return true;
}

bool
ObjectWrapperChild::jsval_to_JSVariant(JSContext* cx, jsval from, JSVariant* to)
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
        // fall through
    case JSTYPE_OBJECT:
        return JSObject_to_JSVariant(cx, JSVAL_TO_OBJECT(from), to);
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
        // fall through
    default:
        return false;
    }
}

/*static*/ bool
ObjectWrapperChild::
JSObject_from_PObjectWrapperChild(JSContext*,
                                  const PObjectWrapperChild* from,
                                  JSObject** to)
{
    const ObjectWrapperChild* owc =
        static_cast<const ObjectWrapperChild*>(from);
    *to = owc ? owc->mObj : NULL;
    return true;
}
    
/*static*/ bool
ObjectWrapperChild::JSObject_from_JSVariant(JSContext* cx,
                                            const JSVariant& from,
                                            JSObject** to)
{
    if (from.type() != JSVariant::TPObjectWrapperChild)
        return false;
    return JSObject_from_PObjectWrapperChild(cx,
                                             from.get_PObjectWrapperChild(),
                                             to);
}

/*static*/ bool
ObjectWrapperChild::jsval_from_JSVariant(JSContext* cx, const JSVariant& from,
                                         jsval* to)
{
    switch (from.type()) {
    case JSVariant::Tvoid_t:
        *to = JSVAL_VOID;
        return true;
    case JSVariant::TPObjectWrapperChild:
        {
            JSObject* obj;
            if (!JSObject_from_JSVariant(cx, from, &obj))
                return false;
            *to = OBJECT_TO_JSVAL(obj);
            return true;
        }
    case JSVariant::TnsString:
        {
            const nsString& str = from.get_nsString();
            JSString* s = JS_NewUCStringCopyN(cx,
                                              str.BeginReading(),
                                              str.Length());
            if (!s)
                return false;
            *to = STRING_TO_JSVAL(s);
        }
        return true;
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
    
ContextWrapperChild*
ObjectWrapperChild::Manager()
{
    PContextWrapperChild* pcwc = PObjectWrapperChild::Manager();
    return static_cast<ContextWrapperChild*>(pcwc);
}

static bool
jsid_to_nsString(JSContext* cx, jsid from, nsString* to)
{
    if (JSID_IS_STRING(from)) {
        size_t length;
        const jschar* chars = JS_GetInternedStringCharsAndLength(JSID_TO_STRING(from), &length);
        *to = nsDependentString(chars, length);
        return true;
    }
    return false;
}
    
static bool
jsid_from_nsString(JSContext* cx, const nsString& from, jsid* to)
{
    JSString* str = JS_NewUCStringCopyN(cx, from.BeginReading(), from.Length());
    if (!str)
        return false;
    return JS_ValueToId(cx, STRING_TO_JSVAL(str), to);
}

#if 0
// The general schema for ObjectWrapperChild::Answer* methods:
bool
ObjectWrapperChild::AnswerSomething(/* in-parameters */
                                    /* out-parameters */)
{
    // initialize out-parameters for failure
    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    // validate in-parameters, else return false
    // successfully perform local JS operations, else return true
    // perform out-parameter conversions, else return false
    return true;
}
// There's an important subtlety here: though a local JS operation may
// fail, leaving out-parameters uninitialized, we must initialize all
// out-parameters when reporting success (returning true) to the IPC
// messaging system.  See AnswerGetProperty for illustration.
#endif

bool
ObjectWrapperChild::AnswerAddProperty(const nsString& id,
                                      OperationStatus* status)
{
    jsid interned_id;

    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    AutoCheckOperation aco(this, status);

    if (!jsid_from_nsString(cx, id, &interned_id))
        return false;
    
    *status = JS_DefinePropertyById(cx, mObj, interned_id, JSVAL_VOID,
                                    NULL, NULL, 0);
    return true;
}

bool
ObjectWrapperChild::AnswerGetProperty(const nsString& id,
                                      OperationStatus* status, JSVariant* vp)
{
    jsid interned_id;
    jsval val;

    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    AutoCheckOperation aco(this, status);

    if (!jsid_from_nsString(cx, id, &interned_id))
        return false;

    *status = JS_GetPropertyById(cx, mObj, interned_id, &val);

    // Since we fully expect this call to jsval_to_JSVariant to return
    // true, we can't just leave vp uninitialized when JS_GetPropertyById
    // returns JS_FALSE.  This pitfall could be avoided in general if IPDL
    // ensured that outparams were pre-initialized to some default value
    // (XXXfixme cjones?).
    return jsval_to_JSVariant(cx, aco.Ok() ? val : JSVAL_VOID, vp);
}

bool
ObjectWrapperChild::AnswerSetProperty(const nsString& id, const JSVariant& v,
                                      OperationStatus* status, JSVariant* vp)
{
    jsid interned_id;
    jsval val;

    *vp = v;

    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    AutoCheckOperation aco(this, status);

    if (!jsid_from_nsString(cx, id, &interned_id) ||
        !jsval_from_JSVariant(cx, v, &val))
        return false;

    *status = JS_SetPropertyById(cx, mObj, interned_id, &val);

    return jsval_to_JSVariant(cx, aco.Ok() ? val : JSVAL_VOID, vp);
}

bool
ObjectWrapperChild::AnswerDelProperty(const nsString& id,
                                      OperationStatus* status, JSVariant* vp)
{
    jsid interned_id;
    jsval val;

    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    AutoCheckOperation aco(this, status);

    if (!jsid_from_nsString(cx, id, &interned_id))
        return false;

    *status = JS_DeletePropertyById2(cx, mObj, interned_id, &val);

    return jsval_to_JSVariant(cx, aco.Ok() ? val : JSVAL_VOID, vp);
}

static const PRUint32 sNextIdIndexSlot = 0;
static const PRUint32 sNumNewEnumerateStateSlots = 1;

static void
CPOW_NewEnumerateState_Finalize(JSContext* cx, JSObject* state)
{
    nsTArray<nsString>* strIds =
        static_cast<nsTArray<nsString>*>(JS_GetPrivate(state));

    if (strIds) {
        delete strIds;
        JS_SetPrivate(state, NULL);
    }
}

// Similar to IteratorClass in XPCWrapper.cpp
static const JSClass sCPOW_NewEnumerateState_JSClass = {
    "CPOW NewEnumerate State",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(sNumNewEnumerateStateSlots),
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
    JS_ConvertStub,   CPOW_NewEnumerateState_Finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

bool
ObjectWrapperChild::AnswerNewEnumerateInit(/* no in-parameters */
                                           OperationStatus* status, JSVariant* statep, int* idp)
{
    *idp = 0;

    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    AutoCheckOperation aco(this, status);

    JSClass* clasp = const_cast<JSClass*>(&sCPOW_NewEnumerateState_JSClass);
    JSObject* state = JS_NewObjectWithGivenProto(cx, clasp, NULL, NULL);
    if (!state)
        return false;
    AutoObjectRooter tvr(cx, state);

    for (JSObject* proto = mObj;
         proto;
         proto = JS_GetPrototype(proto))
    {
        AutoIdArray ids(cx, JS_Enumerate(cx, proto));
        for (size_t i = 0; i < ids.length(); ++i)
            JS_DefinePropertyById(cx, state, ids[i], JSVAL_VOID,
                                  NULL, NULL, JSPROP_ENUMERATE | JSPROP_SHARED);
    }

    InfallibleTArray<nsString>* strIds;
    {
        AutoIdArray ids(cx, JS_Enumerate(cx, state));
        if (!ids)
            return false;
        strIds = new InfallibleTArray<nsString>(ids.length());
        for (size_t i = 0; i < ids.length(); ++i)
            if (!jsid_to_nsString(cx, ids[i], strIds->AppendElement())) {
                delete strIds;
                return false;
            }
    }
    *idp = strIds->Length();

    JS_SetPrivate(state, strIds);
    JS_SetReservedSlot(state, sNextIdIndexSlot, JSVAL_ZERO);
               
    *status = JSObject_to_JSVariant(cx, state, statep);

    return true;
}

bool
ObjectWrapperChild::AnswerNewEnumerateNext(const JSVariant& in_state,
                                           OperationStatus* status, JSVariant* statep, nsString* idp)
{
    JSObject* state;

    *statep = in_state;
    idp->Truncate();
    
    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    AutoCheckOperation aco(this, status);

    if (!JSObject_from_JSVariant(cx, in_state, &state))
        return false;

    InfallibleTArray<nsString>* strIds =
        static_cast<InfallibleTArray<nsString>*>(JS_GetPrivate(state));

    if (!strIds)
        return false;

    jsval v = JS_GetReservedSlot(state, sNextIdIndexSlot);

    int32_t i = JSVAL_TO_INT(v);
    NS_ASSERTION(i >= 0, "Index of next jsid negative?");
    NS_ASSERTION(i <= strIds->Length(), "Index of next jsid too large?");

    if (size_t(i) == strIds->Length()) {
        *status = JS_TRUE;
        return JSObject_to_JSVariant(cx, NULL, statep);
    }

    *idp = strIds->ElementAt(i);
    JS_SetReservedSlot(state, sNextIdIndexSlot, INT_TO_JSVAL(i + 1));
    *status = JS_TRUE;
    return true;
}
    
bool
ObjectWrapperChild::RecvNewEnumerateDestroy(const JSVariant& in_state)
{
    JSObject* state;

    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);

    if (!JSObject_from_JSVariant(cx, in_state, &state))
        return false;

    CPOW_NewEnumerateState_Finalize(cx, state);

    return true;
}

bool
ObjectWrapperChild::AnswerNewResolve(const nsString& id, const int& flags,
                                     OperationStatus* status, PObjectWrapperChild** obj2)
{
    jsid interned_id;
    
    *obj2 = NULL;

    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    AutoCheckOperation aco(this, status);

    if (!jsid_from_nsString(cx, id, &interned_id))
        return false;

    CPOW_LOG(("new-resolving \"%s\"...",
              NS_ConvertUTF16toUTF8(id).get()));

    JSPropertyDescriptor desc;
    if (!JS_GetPropertyDescriptorById(cx, mObj, interned_id, flags, &desc))
        return true;

    *status = JS_TRUE;

    if (desc.obj)
        *obj2 = Manager()->GetOrCreateWrapper(desc.obj);

    return true;
}

bool
ObjectWrapperChild::AnswerConvert(const JSType& type,
                                  OperationStatus* status, JSVariant* vp)
{
    jsval v;
    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    AutoCheckOperation aco(this, status);
    *status = JS_ConvertValue(cx, OBJECT_TO_JSVAL(mObj), type, &v);
    return jsval_to_JSVariant(cx, aco.Ok() ? v : JSVAL_VOID, vp);
}

namespace {
    // Should be an overestimate of typical JS function arity.
    typedef nsAutoTArray<jsval, 5> AutoJSArgs;
}

bool
ObjectWrapperChild::AnswerCall(PObjectWrapperChild* receiver, const InfallibleTArray<JSVariant>& argv,
                               OperationStatus* status, JSVariant* rval)
{
    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    AutoCheckOperation aco(this, status);

    JSObject* obj;
    if (!JSObject_from_PObjectWrapperChild(cx, receiver, &obj))
        return false;

    AutoJSArgs args;
    PRUint32 argc = argv.Length();
    jsval *jsargs = args.AppendElements(argc);
    if (!jsargs)
        return false;
    AutoArrayRooter tvr(cx, argc, jsargs);

    for (PRUint32 i = 0; i < argc; ++i)
        if (!jsval_from_JSVariant(cx, argv.ElementAt(i), jsargs + i))
            return false;

    jsval rv;
    *status = JS_CallFunctionValue(cx, obj, OBJECT_TO_JSVAL(mObj),
                                   argv.Length(), jsargs, &rv);

    return jsval_to_JSVariant(cx, aco.Ok() ? rv : JSVAL_VOID, rval);
}

bool
ObjectWrapperChild::AnswerConstruct(const InfallibleTArray<JSVariant>& argv,
                                    OperationStatus* status, PObjectWrapperChild** rval)
{
    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    AutoCheckOperation aco(this, status);

    AutoJSArgs args;
    PRUint32 argc = argv.Length();
    jsval* jsargs = args.AppendElements(argc);
    if (!jsargs)
        return false;
    AutoArrayRooter tvr(cx, argc, jsargs);

    for (PRUint32 i = 0; i < argc; ++i)
        if (!jsval_from_JSVariant(cx, argv.ElementAt(i), jsargs + i))
            return false;

    JSObject* obj = JS_New(cx, mObj, argc, jsargs);

    *status = !!obj;
    *rval = Manager()->GetOrCreateWrapper(obj);

    return true;
}

bool
ObjectWrapperChild::AnswerHasInstance(const JSVariant& v,
                                      OperationStatus* status, JSBool* bp)
{
    jsval candidate;
    JSContext* cx = Manager()->GetContext();
    AutoContextPusher acp(cx);
    AutoCheckOperation aco(this, status);
    if (!jsval_from_JSVariant(cx, v, &candidate))
        return false;
    *status = JS_HasInstance(cx, mObj, candidate, bp);
    return true;
}
