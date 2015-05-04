/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ProcessGlobal_h
#define mozilla_dom_ProcessGlobal_h

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsFrameMessageManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIClassInfo.h"
#include "nsIRunnable.h"
#include "nsIGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace dom {

class ProcessGlobal :
  public nsMessageManagerScriptExecutor,
  public nsIContentProcessMessageManager,
  public nsIGlobalObject,
  public nsIScriptObjectPrincipal,
  public nsSupportsWeakReference,
  public mozilla::dom::ipc::MessageManagerCallback,
  public nsWrapperCache
{
public:
  explicit ProcessGlobal(nsFrameMessageManager* aMessageManager);

  bool Init();

  static ProcessGlobal* Get();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(ProcessGlobal, nsIContentProcessMessageManager)

  NS_FORWARD_SAFE_NSIMESSAGELISTENERMANAGER(mMessageManager)
  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)
  NS_FORWARD_SAFE_NSISYNCMESSAGESENDER(mMessageManager)
  NS_FORWARD_SAFE_NSIMESSAGEMANAGERGLOBAL(mMessageManager)

  virtual void LoadScript(const nsAString& aURL);

  virtual JSObject* GetGlobalJSObject() override
  {
    if (!mGlobal) {
      return nullptr;
    }

    return mGlobal->GetJSObject();
  }
  virtual nsIPrincipal* GetPrincipal() override { return mPrincipal; }

  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override
  {
    MOZ_CRASH("ProcessGlobal doesn't use DOM bindings!");
  }

protected:
  virtual ~ProcessGlobal();

private:
  bool mInitialized;
  nsRefPtr<nsFrameMessageManager> mMessageManager;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ProcessGlobal_h
