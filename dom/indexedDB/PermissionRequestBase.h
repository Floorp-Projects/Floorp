/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_permissionrequestbase_h__
#define mozilla_dom_indexeddb_permissionrequestbase_h__

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsIPermissionManager.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

class nsIPrincipal;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {
namespace indexedDB {

class PermissionRequestBase
  : public nsIObserver
  , public nsIInterfaceRequestor
{
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsIPrincipal> mPrincipal;

public:
  enum PermissionValue {
    kPermissionAllowed = nsIPermissionManager::ALLOW_ACTION,
    kPermissionDenied = nsIPermissionManager::DENY_ACTION,
    kPermissionPrompt = nsIPermissionManager::PROMPT_ACTION
  };

  NS_DECL_ISUPPORTS

  // This function will not actually prompt. It will never return
  // kPermissionDefault but will instead translate the permission manager value
  // into the correct value for the given type.
  static nsresult
  GetCurrentPermission(nsIPrincipal* aPrincipal,
                       PermissionValue* aCurrentValue);

  static PermissionValue
  PermissionValueForIntPermission(uint32_t aIntPermission);

  // This function will prompt if needed. It may only be called once.
  nsresult
  PromptIfNeeded(PermissionValue* aCurrentValue);

protected:
  PermissionRequestBase(nsPIDOMWindow* aWindow,
                        nsIPrincipal* aPrincipal);

  // Reference counted.
  virtual
  ~PermissionRequestBase();

  virtual void
  OnPromptComplete(PermissionValue aPermissionValue) = 0;

private:
  void
  SetExplicitPermission(nsIPrincipal* aPrincipal,
                        uint32_t aIntPermission);

  NS_DECL_NSIOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_permissionrequestbase_h__
