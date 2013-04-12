/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_ContextWrapperParent_h__
#define mozilla_jsipc_ContextWrapperParent_h__

#include "mozilla/jsipc/PContextWrapperParent.h"
#include "mozilla/jsipc/ObjectWrapperParent.h"
#include "mozilla/jsipc/CPOWTypes.h"

#include "mozilla/dom/ContentParent.h"

#include "jsapi.h"
#include "nsAutoJSValHolder.h"

namespace mozilla {
namespace jsipc {

using mozilla::dom::ContentParent;
    
class ContextWrapperParent
    : public PContextWrapperParent
{
public:

    ContextWrapperParent(ContentParent* cpp)
        : mContent(cpp)
        , mGlobal(NULL)
    {}

    JSBool GetGlobalJSObject(JSContext* cx, JSObject** globalp) {
        if (!mGlobal)
            return JS_FALSE;
        mGlobalHolder.Hold(cx);
        mGlobalHolder = *globalp = mGlobal->GetJSObject(cx);
        return JS_TRUE;
    }

    ObjectWrapperParent* GetGlobalObjectWrapper() const {
        return mGlobal;
    }

    bool RequestRunToCompletion() {
        return mContent->RequestRunToCompletion();
    }

private:

    ContentParent* mContent;
    ObjectWrapperParent* mGlobal;
    nsAutoJSValHolder mGlobalHolder;

    PObjectWrapperParent* AllocPObjectWrapper(const bool&) {
        return new ObjectWrapperParent();
    }

    bool RecvPObjectWrapperConstructor(PObjectWrapperParent* actor,
                                       const bool& makeGlobal)
    {
        if (makeGlobal) {
            mGlobalHolder.Release();
            mGlobal = static_cast<ObjectWrapperParent*>(actor);
        }
        return true;
    }

    bool DeallocPObjectWrapper(PObjectWrapperParent* actor)
    {
        if (mGlobal &&
            mGlobal == static_cast<ObjectWrapperParent*>(actor)) {
            mGlobalHolder.Release();
            mGlobal = NULL;
        }
        delete actor;
        return true;
    }

};

}}

#endif
