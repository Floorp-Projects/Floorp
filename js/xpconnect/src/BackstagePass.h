/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BackstagePass_h__
#define BackstagePass_h__

#include "nsISupports.h"
#include "nsWeakReference.h"
#include "nsIGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIXPCScriptable.h"

class BackstagePass : public nsIGlobalObject,
                      public nsIScriptObjectPrincipal,
                      public nsIXPCScriptable,
                      public nsIClassInfo,
                      public nsSupportsWeakReference
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIXPCSCRIPTABLE
  NS_DECL_NSICLASSINFO

  virtual nsIPrincipal* GetPrincipal() {
    return mPrincipal;
  }

  virtual JSObject* GetGlobalJSObject() {
    return mGlobal;
  }

  virtual void ForgetGlobalObject() {
    mGlobal = nullptr;
  }

  virtual void SetGlobalObject(JSObject* global) {
    mGlobal = global;
  }

  BackstagePass(nsIPrincipal *prin) :
    mPrincipal(prin)
  {
  }

  virtual ~BackstagePass() { }

private:
  nsCOMPtr<nsIPrincipal> mPrincipal;
  JSObject *mGlobal;
};

NS_EXPORT nsresult
NS_NewBackstagePass(BackstagePass** ret);

#endif // BackstagePass_h__
