/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SANDBOXPRIVATE_H__
#define __SANDBOXPRIVATE_H__

#include "nsIGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIPrincipal.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"

#include "js/RootingAPI.h"


class SandboxPrivate : public nsIGlobalObject,
                       public nsIScriptObjectPrincipal,
                       public nsSupportsWeakReference,
                       public nsWrapperCache
{
public:
    SandboxPrivate(nsIPrincipal* principal, JSObject* global)
        : mPrincipal(principal)
    {
        SetIsNotDOMBinding();
        SetWrapper(global);
    }

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(SandboxPrivate,
                                                           nsIGlobalObject)

    nsIPrincipal* GetPrincipal() override
    {
        return mPrincipal;
    }

    JSObject* GetGlobalJSObject() override
    {
        return GetWrapper();
    }

    void ForgetGlobalObject()
    {
        ClearWrapper();
    }

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override
    {
        MOZ_CRASH("SandboxPrivate doesn't use DOM bindings!");
    }

    void ObjectMoved(JSObject* obj, const JSObject* old)
    {
        UpdateWrapper(obj, old);
    }

private:
    virtual ~SandboxPrivate() { }

    nsCOMPtr<nsIPrincipal> mPrincipal;
};

#endif // __SANDBOXPRIVATE_H__
