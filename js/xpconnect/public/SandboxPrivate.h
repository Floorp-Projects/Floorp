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

// This interface is public only because it is used in jsd.
// Once jsd is gone this file should be moved back to xpconnect/src.

class SandboxPrivate : public nsIGlobalObject,
                       public nsIScriptObjectPrincipal,
                       public nsSupportsWeakReference,
                       public nsWrapperCache
{
public:
    SandboxPrivate(nsIPrincipal *principal, JSObject *global)
        : mPrincipal(principal)
    {
        SetWrapper(global);
    }

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(SandboxPrivate,
                                                           nsIGlobalObject)

    nsIPrincipal *GetPrincipal()
    {
        return mPrincipal;
    }

    JSObject *GetGlobalJSObject()
    {
        return GetWrapper();
    }

    void ForgetGlobalObject()
    {
        ClearWrapper();
    }

private:
    virtual ~SandboxPrivate() { }

    nsCOMPtr<nsIPrincipal> mPrincipal;
};

#endif // __SANDBOXPRIVATE_H__
