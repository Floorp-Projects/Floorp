/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SANDBOXPRIVATE_H__
#define __SANDBOXPRIVATE_H__

#include "nsIGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIPrincipal.h"
#include "nsWeakReference.h"

// This interface is public only because it is used in jsd.
// Once jsd is gone this file should be moved back to xpconnect/src.

class SandboxPrivate : public nsIGlobalObject,
                       public nsIScriptObjectPrincipal,
                       public nsSupportsWeakReference
{
public:
    SandboxPrivate(nsIPrincipal *principal, JSObject *global)
        : mPrincipal(principal)
        , mGlobalJSObject(global)
    {
    }
    virtual ~SandboxPrivate() { }

    NS_DECL_ISUPPORTS

    nsIPrincipal *GetPrincipal()
    {
        return mPrincipal;
    }

    JSObject *GetGlobalJSObject()
    {
        return mGlobalJSObject;
    }

    void ForgetGlobalObject()
    {
        mGlobalJSObject = nullptr;
    }
private:
    nsCOMPtr<nsIPrincipal> mPrincipal;
    JSObject *mGlobalJSObject;
};

#endif // __SANDBOXPRIVATE_H__
