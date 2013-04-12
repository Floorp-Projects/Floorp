/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_ObjectWrapperChild_h__
#define mozilla_jsipc_ObjectWrapperChild_h__

#include "mozilla/jsipc/PObjectWrapperChild.h"

// For OperationChecker and AutoCheckOperationBase.
#include "mozilla/jsipc/ObjectWrapperParent.h"

using mozilla::jsipc::JSVariant;

namespace mozilla {
namespace jsipc {

class ContextWrapperChild;

class ObjectWrapperChild
    : public PObjectWrapperChild
    , public OperationChecker
{
public:

    ObjectWrapperChild(JSContext* cx, JSObject* obj);

    JSObject* GetJSObject() const { return mObj; }

    void CheckOperation(JSContext* cx, OperationStatus* status);
    
private:

    JSObject* mObj;

    bool JSObject_to_JSVariant(JSContext* cx, JSObject* from, JSVariant* to);
    bool jsval_to_JSVariant(JSContext* cx, jsval from, JSVariant* to);

    static bool JSObject_from_PObjectWrapperChild(JSContext* cx,
                                                  const PObjectWrapperChild* from,
                                                  JSObject** to);
    static bool JSObject_from_JSVariant(JSContext* cx, const JSVariant& from,
                                        JSObject** to);
    static bool jsval_from_JSVariant(JSContext* cx, const JSVariant& from,
                                     jsval* to);

    ContextWrapperChild* Manager();

protected:

    void ActorDestroy(ActorDestroyReason why);

    bool AnswerAddProperty(const nsString& id,
                           OperationStatus* status);

    bool AnswerGetProperty(const nsString& id,
                           OperationStatus* status, JSVariant* vp);

    bool AnswerSetProperty(const nsString& id, const JSVariant& v,
                           OperationStatus* status, JSVariant* vp);

    bool AnswerDelProperty(const nsString& id,
                           OperationStatus* status, JSVariant* vp);

    bool AnswerNewEnumerateInit(/* no in-parameters */
                                OperationStatus* status, JSVariant* statep, int* idp);

    bool AnswerNewEnumerateNext(const JSVariant& in_state,
                                OperationStatus* status, JSVariant* statep, nsString* idp);

    bool RecvNewEnumerateDestroy(const JSVariant& in_state);

    bool AnswerNewResolve(const nsString& id, const int& flags,
                          OperationStatus* status, PObjectWrapperChild** obj2);

    bool AnswerConvert(const JSType& type,
                       OperationStatus* status, JSVariant* vp);

    bool AnswerCall(PObjectWrapperChild* receiver, const InfallibleTArray<JSVariant>& argv,
                    OperationStatus* status, JSVariant* rval);

    bool AnswerConstruct(const InfallibleTArray<JSVariant>& argv,
                         OperationStatus* status, PObjectWrapperChild** rval);

    bool AnswerHasInstance(const JSVariant& v,
                           OperationStatus* status, JSBool* bp);
};

}}
  
#endif
