/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentPermissionHelper.h"
#include "nsIContentPermissionPrompt.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/Element.h"
#include "mozilla/unused.h"
#include "nsComponentManagerUtils.h"

using mozilla::unused;          // <snicker>
using namespace mozilla::dom;

nsContentPermissionRequestProxy::nsContentPermissionRequestProxy()
{
  MOZ_COUNT_CTOR(nsContentPermissionRequestProxy);
}

nsContentPermissionRequestProxy::~nsContentPermissionRequestProxy()
{
  MOZ_COUNT_DTOR(nsContentPermissionRequestProxy);
}

nsresult
nsContentPermissionRequestProxy::Init(const nsACString & type,
                                      const nsACString & access,
                                      ContentPermissionRequestParent* parent)
{
  NS_ASSERTION(parent, "null parent");
  mParent = parent;
  mType   = type;
  mAccess = access;

  nsCOMPtr<nsIContentPermissionPrompt> prompt = do_CreateInstance(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
  if (!prompt) {
    return NS_ERROR_FAILURE;
  }

  prompt->Prompt(this);
  return NS_OK;
}

void
nsContentPermissionRequestProxy::OnParentDestroyed()
{
  mParent = nullptr;
}

NS_IMPL_ISUPPORTS1(nsContentPermissionRequestProxy, nsIContentPermissionRequest)

NS_IMETHODIMP
nsContentPermissionRequestProxy::GetType(nsACString & aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::GetAccess(nsACString & aAccess)
{
  aAccess = mAccess;
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::GetWindow(nsIDOMWindow * *aRequestingWindow)
{
  NS_ENSURE_ARG_POINTER(aRequestingWindow);
  *aRequestingWindow = nullptr; // ipc doesn't have a window
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::GetPrincipal(nsIPrincipal * *aRequestingPrincipal)
{
  NS_ENSURE_ARG_POINTER(aRequestingPrincipal);
  if (mParent == nullptr) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aRequestingPrincipal = mParent->mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::GetElement(nsIDOMElement * *aRequestingElement)
{
  NS_ENSURE_ARG_POINTER(aRequestingElement);
  if (mParent == nullptr) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(mParent->mElement);
  elem.forget(aRequestingElement);
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::Cancel()
{
  if (mParent == nullptr) {
    return NS_ERROR_FAILURE;
  }

  unused << ContentPermissionRequestParent::Send__delete__(mParent, false);
  mParent = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::Allow()
{
  if (mParent == nullptr) {
    return NS_ERROR_FAILURE;
  }
  unused << ContentPermissionRequestParent::Send__delete__(mParent, true);
  mParent = nullptr;
  return NS_OK;
}

namespace mozilla {
namespace dom {

ContentPermissionRequestParent::ContentPermissionRequestParent(const nsACString& aType,
                                                               const nsACString& aAccess,
                                                               Element* aElement,
                                                               const IPC::Principal& aPrincipal)
{
  MOZ_COUNT_CTOR(ContentPermissionRequestParent);

  mPrincipal = aPrincipal;
  mElement   = aElement;
  mType      = aType;
  mAccess    = aAccess;
}

ContentPermissionRequestParent::~ContentPermissionRequestParent()
{
  MOZ_COUNT_DTOR(ContentPermissionRequestParent);
}

bool
ContentPermissionRequestParent::Recvprompt()
{
  mProxy = new nsContentPermissionRequestProxy();
  NS_ASSERTION(mProxy, "Alloc of request proxy failed");
  if (NS_FAILED(mProxy->Init(mType, mAccess, this))) {
    mProxy->Cancel();
  }
  return true;
}

void
ContentPermissionRequestParent::ActorDestroy(ActorDestroyReason why)
{
  if (mProxy) {
    mProxy->OnParentDestroyed();
  }
}

} // namespace dom
} // namespace mozilla
