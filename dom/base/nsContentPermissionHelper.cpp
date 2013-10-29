/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/PContentPermission.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/PContentPermissionRequestParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/unused.h"
#include "nsComponentManagerUtils.h"
#include "nsArrayUtils.h"
#include "nsIMutableArray.h"
#include "nsContentPermissionHelper.h"

using mozilla::unused;          // <snicker>
using namespace mozilla::dom;

namespace mozilla {
namespace dom {

class ContentPermissionRequestParent : public PContentPermissionRequestParent
{
 public:
  ContentPermissionRequestParent(const nsTArray<PermissionRequest>& aRequests,
                                 Element* element,
                                 const IPC::Principal& principal);
  virtual ~ContentPermissionRequestParent();

  bool IsBeingDestroyed();

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<Element> mElement;
  nsCOMPtr<nsContentPermissionRequestProxy> mProxy;
  nsTArray<PermissionRequest> mRequests;

 private:
  virtual bool Recvprompt();
  virtual void ActorDestroy(ActorDestroyReason why);
};

ContentPermissionRequestParent::ContentPermissionRequestParent(const nsTArray<PermissionRequest>& aRequests,
                                                               Element* aElement,
                                                               const IPC::Principal& aPrincipal)
{
  MOZ_COUNT_CTOR(ContentPermissionRequestParent);

  mPrincipal = aPrincipal;
  mElement   = aElement;
  mRequests  = aRequests;
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
  if (NS_FAILED(mProxy->Init(mRequests, this))) {
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

bool
ContentPermissionRequestParent::IsBeingDestroyed()
{
  // When TabParent::Destroy() is called, we are being destroyed. It's unsafe
  // to send out any message now.
  TabParent* tabParent = static_cast<TabParent*>(Manager());
  return tabParent->IsDestroyed();
}

NS_IMPL_ISUPPORTS1(ContentPermissionType, nsIContentPermissionType)

ContentPermissionType::ContentPermissionType(const nsACString& aType,
                                             const nsACString& aAccess)
{
  mType = aType;
  mAccess = aAccess;
}

ContentPermissionType::~ContentPermissionType()
{
}

NS_IMETHODIMP
ContentPermissionType::GetType(nsACString& aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
ContentPermissionType::GetAccess(nsACString& aAccess)
{
  aAccess = mAccess;
  return NS_OK;
}

uint32_t
ConvertPermissionRequestToArray(nsTArray<PermissionRequest>& aSrcArray,
                                nsIMutableArray* aDesArray)
{
  uint32_t len = aSrcArray.Length();
  for (uint32_t i = 0; i < len; i++) {
    nsRefPtr<ContentPermissionType> cpt =
      new ContentPermissionType(aSrcArray[i].type(), aSrcArray[i].access());
    aDesArray->AppendElement(cpt, false);
  }
  return len;
}

nsresult
CreatePermissionArray(const nsACString& aType,
                      const nsACString& aAccess,
                      nsIArray** aTypesArray)
{
  nsCOMPtr<nsIMutableArray> types = do_CreateInstance(NS_ARRAY_CONTRACTID);
  nsRefPtr<ContentPermissionType> permType = new ContentPermissionType(aType,
                                                                       aAccess);
  types->AppendElement(permType, false);
  types.forget(aTypesArray);

  return NS_OK;
}

PContentPermissionRequestParent*
CreateContentPermissionRequestParent(const nsTArray<PermissionRequest>& aRequests,
                                     Element* element,
                                     const IPC::Principal& principal)
{
  return new ContentPermissionRequestParent(aRequests, element, principal);
}

} // namespace dom
} // namespace mozilla

nsContentPermissionRequestProxy::nsContentPermissionRequestProxy()
{
  MOZ_COUNT_CTOR(nsContentPermissionRequestProxy);
}

nsContentPermissionRequestProxy::~nsContentPermissionRequestProxy()
{
  MOZ_COUNT_DTOR(nsContentPermissionRequestProxy);
}

nsresult
nsContentPermissionRequestProxy::Init(const nsTArray<PermissionRequest>& requests,
                                      ContentPermissionRequestParent* parent)
{
  NS_ASSERTION(parent, "null parent");
  mParent = parent;
  mPermissionRequests = requests;

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
nsContentPermissionRequestProxy::GetTypes(nsIArray** aTypes)
{
  nsCOMPtr<nsIMutableArray> types = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (ConvertPermissionRequestToArray(mPermissionRequests, types)) {
    types.forget(aTypes);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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

  // Don't send out the delete message when the managing protocol (PBrowser) is
  // being destroyed and PContentPermissionRequest will soon be.
  if (mParent->IsBeingDestroyed()) {
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

  // Don't send out the delete message when the managing protocol (PBrowser) is
  // being destroyed and PContentPermissionRequest will soon be.
  if (mParent->IsBeingDestroyed()) {
    return NS_ERROR_FAILURE;
  }

  unused << ContentPermissionRequestParent::Send__delete__(mParent, true);
  mParent = nullptr;
  return NS_OK;
}
