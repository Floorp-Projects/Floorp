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

#ifndef mozilla_jsipc_ObjectWrapperParent_h__
#define mozilla_jsipc_ObjectWrapperParent_h__

#include "mozilla/jsipc/PObjectWrapperParent.h"
#include "jsapi.h"
#include "jsvalue.h"
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
    CPOW_AddProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

    static JSBool
    CPOW_DelProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

    static JSBool
    CPOW_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);
    
    static JSBool
    CPOW_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

    JSBool NewEnumerateInit(JSContext* cx, jsval* statep, jsid* idp);
    JSBool NewEnumerateNext(JSContext* cx, jsval* statep, jsid* idp);
    JSBool NewEnumerateDestroy(JSContext* cx, jsval state);
    static JSBool
    CPOW_NewEnumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                      jsval *statep, jsid *idp);

    static JSBool
    CPOW_NewResolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                    JSObject **objp);

    static JSBool
    CPOW_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

    static void
    CPOW_Finalize(JSContext* cx, JSObject* obj);

    static JSBool
    CPOW_Call(JSContext* cx, uintN argc, jsval* vp);

    static JSBool
    CPOW_Construct(JSContext *cx, uintN argc, jsval *vp);
    
    static JSBool
    CPOW_HasInstance(JSContext *cx, JSObject *obj, const jsval *v, JSBool *bp);

    static JSBool
    CPOW_Equality(JSContext *cx, JSObject *obj, const jsval *v, JSBool *bp);

    static bool jsval_to_JSVariant(JSContext* cx, jsval from, JSVariant* to);
    static bool jsval_from_JSVariant(JSContext* cx, const JSVariant& from,
                                     jsval* to);
    static bool
    JSObject_to_PObjectWrapperParent(JSContext* cx, JSObject* from,
                                     PObjectWrapperParent** to);
    static bool
    JSObject_from_PObjectWrapperParent(JSContext* cx,
                                       const PObjectWrapperParent* from,
                                       JSObject** to);
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
