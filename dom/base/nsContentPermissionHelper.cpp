/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentPermissionHelper.h"
#include "nsIContentPermissionPrompt.h"
#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsIDOMElement.h"

#include "mozilla/unused.h"

using mozilla::unused;          // <snicker>

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
				                      mozilla::dom::ContentPermissionRequestParent* parent)
{
  NS_ASSERTION(parent, "null parent");
  mParent = parent;
  mType   = type;

  nsCOMPtr<nsIContentPermissionPrompt> prompt = do_GetService(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
  if (!prompt) {
    return NS_ERROR_FAILURE;
  }

  prompt->Prompt(this);
  return NS_OK;
}

void
nsContentPermissionRequestProxy::OnParentDestroyed()
{
  mParent = nsnull;
}

NS_IMPL_ISUPPORTS1(nsContentPermissionRequestProxy, nsIContentPermissionRequest);

NS_IMETHODIMP
nsContentPermissionRequestProxy::GetType(nsACString & aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::GetWindow(nsIDOMWindow * *aRequestingWindow)
{
  NS_ENSURE_ARG_POINTER(aRequestingWindow);
  *aRequestingWindow = nsnull; // ipc doesn't have a window
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::GetUri(nsIURI * *aRequestingURI)
{
  NS_ENSURE_ARG_POINTER(aRequestingURI);
  if (mParent == nsnull)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aRequestingURI = mParent->mURI);
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::GetElement(nsIDOMElement * *aRequestingElement)
{
  NS_ENSURE_ARG_POINTER(aRequestingElement);
  if (mParent == nsnull)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*aRequestingElement = mParent->mElement);
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::Cancel()
{
  if (mParent == nsnull)
    return NS_ERROR_FAILURE;
  unused << mozilla::dom::ContentPermissionRequestParent::Send__delete__(mParent, false);
  mParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::Allow()
{
  if (mParent == nsnull)
    return NS_ERROR_FAILURE;
  unused << mozilla::dom::ContentPermissionRequestParent::Send__delete__(mParent, true);
  mParent = nsnull;
  return NS_OK;
}

namespace mozilla {
namespace dom {

ContentPermissionRequestParent::ContentPermissionRequestParent(const nsACString& aType,
                                                               nsIDOMElement *aElement,
                                                               const IPC::URI& aUri)
{
  MOZ_COUNT_CTOR(ContentPermissionRequestParent);
  
  mURI       = aUri;
  mElement   = aElement;
  mType      = aType;
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
  if (NS_FAILED(mProxy->Init(mType, this)))
    mProxy->Cancel();
  return true;
}

void
ContentPermissionRequestParent::ActorDestroy(ActorDestroyReason why)
{
  mProxy->OnParentDestroyed();
}

} // namespace dom
} // namespace mozilla
