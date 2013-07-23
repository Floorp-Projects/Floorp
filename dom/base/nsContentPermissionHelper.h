/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentPermissionHelper_h
#define nsContentPermissionHelper_h

#include "base/basictypes.h"

#include "nsIContentPermissionPrompt.h"
#include "nsString.h"

#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/PContentPermissionRequestParent.h"

class nsContentPermissionRequestProxy;

namespace mozilla {
namespace dom {

class Element;

class ContentPermissionRequestParent : public PContentPermissionRequestParent
{
 public:
  ContentPermissionRequestParent(const nsACString& type,
                                 const nsACString& access,
                                 Element* element,
                                 const IPC::Principal& principal);
  virtual ~ContentPermissionRequestParent();

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<Element> mElement;
  nsCOMPtr<nsContentPermissionRequestProxy> mProxy;
  nsCString mType;
  nsCString mAccess;

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

  nsresult Init(const nsACString& type, const nsACString& access, mozilla::dom::ContentPermissionRequestParent* parent);
  void OnParentDestroyed();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST

 private:
  // Non-owning pointer to the ContentPermissionRequestParent object which owns this proxy.
  mozilla::dom::ContentPermissionRequestParent* mParent;
  nsCString mType;
  nsCString mAccess;
};
#endif // nsContentPermissionHelper_h

