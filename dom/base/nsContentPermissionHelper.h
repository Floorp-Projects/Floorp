/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentPermissionHelper_h
#define nsContentPermissionHelper_h

#include "base/basictypes.h"

#include "nsIContentPermissionPrompt.h"
#include "nsString.h"
#include "nsIDOMElement.h"

#include "mozilla/dom/PContentPermissionRequestParent.h"

class nsContentPermissionRequestProxy;

namespace mozilla {
namespace dom {

class ContentPermissionRequestParent : public PContentPermissionRequestParent
{
 public:
  ContentPermissionRequestParent(const nsACString& type, nsIDOMElement *element, const IPC::URI& principal);
  virtual ~ContentPermissionRequestParent();
  
  nsCOMPtr<nsIURI>           mURI;
  nsCOMPtr<nsIDOMElement>    mElement;
  nsCOMPtr<nsContentPermissionRequestProxy> mProxy;
  nsCString mType;

 private:  
  virtual bool Recvprompt();
  virtual void ActorDestroy(ActorDestroyReason why);
};
  
} // namespace dom
} // namespace mozilla

class nsContentPermissionRequestProxy : public nsIContentPermissionRequest
{
 public:
  nsContentPermissionRequestProxy();
  virtual ~nsContentPermissionRequestProxy();
  
  nsresult Init(const nsACString& type, mozilla::dom::ContentPermissionRequestParent* parent);
  void OnParentDestroyed();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST

 private:
  // Non-owning pointer to the ContentPermissionRequestParent object which owns this proxy.
  mozilla::dom::ContentPermissionRequestParent* mParent;
  nsCString mType;
};
#endif // nsContentPermissionHelper_h

