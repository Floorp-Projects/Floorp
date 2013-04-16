/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_ObjectWrapperParent_h__
#define mozilla_jsipc_ObjectWrapperParent_h__

#include "mozilla/jsipc/PObjectWrapperParent.h"
#include "jsapi.h"
#include "jsclass.h"
#include "nsAutoJSValHolder.h"

namespace mozilla {
namespace jsipc {

class ContextWrapperParent;

class OperationChecker {
public:
    virtual void CheckOperation(JSContext* cx,
                                OperationStatus* status) = 0;
};

class ObjectWrapperParent
    : public PObjectWrapperParent
    , public OperationChecker
{
public:

    ObjectWrapperParent()
        : mObj(NULL)
    {}

    JSObject* GetJSObject(JSContext* cx) const;

    JSObject* GetJSObjectOrNull() const {
        return mObj;
    }

    jsval GetJSVal(JSContext* cx) const {
        return OBJECT_TO_JSVAL(GetJSObject(cx));
    }

    void CheckOperation(JSContext* cx,
                        OperationStatus* status);

    static const js::Class sCPOW_JSClass;

protected:

    void ActorDestroy(ActorDestroyReason why);

    ContextWrapperParent* Manager();

private:

    mutable JSObject* mObj;

    static JSBool
    CPOW_AddProperty(JSContext *cx, JSHandleObject obj, JSHandleId id, JSMutableHandleValue vp);

    static JSBool
    CPOW_DelProperty(JSContext *cx, JSHandleObject obj, JSHandleId id, JSBool *succeeded);

    static JSBool
    CPOW_GetProperty(JSContext *cx, JSHandleObject obj, JSHandleId id, JSMutableHandleValue vp);
    
    static JSBool
    CPOW_SetProperty(JSContext *cx, JSHandleObject obj, JSHandleId id, JSBool strict, JSMutableHandleValue vp);

    JSBool NewEnumerateInit(JSContext* cx, jsval* statep, jsid* idp);
    JSBool NewEnumerateNext(JSContext* cx, jsval* statep, jsid* idp);
    JSBool NewEnumerateDestroy(JSContext* cx, jsval state);
    static JSBool
    CPOW_NewEnumerate(JSContext *cx, JSHandleObject obj, JSIterateOp enum_op,
                      jsval *statep, jsid *idp);

    static JSBool
    CPOW_NewResolve(JSContext *cx, JSHandleObject obj, JSHandleId id, unsigned flags,
                    JSMutableHandleObject objp);

    static JSBool
    CPOW_Convert(JSContext *cx, JSHandleObject obj, JSType type, JSMutableHandleValue vp);

    static void
    CPOW_Finalize(js::FreeOp* fop, JSObject* obj);

    static JSBool
    CPOW_Call(JSContext* cx, unsigned argc, jsval* vp);

    static JSBool
    CPOW_Construct(JSContext *cx, unsigned argc, jsval *vp);

    static JSBool
    CPOW_HasInstance(JSContext *cx, JSHandleObject obj, JSMutableHandleValue vp, JSBool *bp);

    static bool jsval_to_JSVariant(JSContext* cx, jsval from, JSVariant* to);
    static bool jsval_from_JSVariant(JSContext* cx, const JSVariant& from,
                                     jsval* to);
    static bool boolean_from_JSVariant(JSContext* cx, const JSVariant& from,
                                       JSBool* to);
    static bool
    JSObject_to_PObjectWrapperParent(JSContext* cx, JSObject* from, PObjectWrapperParent** to);
    static bool
    JSObject_from_PObjectWrapperParent(JSContext* cx,
                                       const PObjectWrapperParent* from,
                                       JSMutableHandleObject to);
    static bool
    jsval_from_PObjectWrapperParent(JSContext* cx,
                                    const PObjectWrapperParent* from,
                                    jsval* to);
};

template <class StatusOwnerPolicy>
class AutoCheckOperationBase
    : public StatusOwnerPolicy
{
    JSContext* const mContext;
    OperationChecker* const mChecker;

protected:

    AutoCheckOperationBase(JSContext* cx,
                           OperationChecker* checker)
        : mContext(cx)
        , mChecker(checker)
    {}

    virtual ~AutoCheckOperationBase() {
        mChecker->CheckOperation(mContext, StatusOwnerPolicy::StatusPtr());
    }

public:

    bool Ok() {
        return (StatusOwnerPolicy::StatusPtr()->type() == OperationStatus::TJSBool &&
                StatusOwnerPolicy::StatusPtr()->get_JSBool());
    }
};

}}

#endif
