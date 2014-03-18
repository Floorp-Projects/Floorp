/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GONK
#include "GonkPermission.h"
#include "mozilla/dom/ContentParent.h"
#endif // MOZ_WIDGET_GONK
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
#include "nsCxPusher.h"
#include "nsJSUtils.h"
#include "nsISupportsPrimitives.h"

using mozilla::unused;          // <snicker>
using namespace mozilla::dom;
using namespace mozilla;

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
  nsRefPtr<nsContentPermissionRequestProxy> mProxy;
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
                                             const nsACString& aAccess,
                                             const nsTArray<nsString>& aOptions)
{
  mType = aType;
  mAccess = aAccess;
  mOptions = aOptions;
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

NS_IMETHODIMP
ContentPermissionType::GetOptions(nsIArray** aOptions)
{
  NS_ENSURE_ARG_POINTER(aOptions);

  *aOptions = nullptr;

  nsresult rv;
  nsCOMPtr<nsIMutableArray> options =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // copy options into JS array
  for (uint32_t i = 0; i < mOptions.Length(); ++i) {
    nsCOMPtr<nsISupportsString> isupportsString =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = isupportsString->SetData(mOptions[i]);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = options->AppendElement(isupportsString, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ADDREF(*aOptions = options);
  return NS_OK;
}

uint32_t
ConvertPermissionRequestToArray(nsTArray<PermissionRequest>& aSrcArray,
                                nsIMutableArray* aDesArray)
{
  uint32_t len = aSrcArray.Length();
  for (uint32_t i = 0; i < len; i++) {
    nsRefPtr<ContentPermissionType> cpt =
      new ContentPermissionType(aSrcArray[i].type(),
                                aSrcArray[i].access(),
                                aSrcArray[i].options());
    aDesArray->AppendElement(cpt, false);
  }
  return len;
}

nsresult
CreatePermissionArray(const nsACString& aType,
                      const nsACString& aAccess,
                      const nsTArray<nsString>& aOptions,
                      nsIArray** aTypesArray)
{
  nsCOMPtr<nsIMutableArray> types = do_CreateInstance(NS_ARRAY_CONTRACTID);
  nsRefPtr<ContentPermissionType> permType = new ContentPermissionType(aType,
                                                                       aAccess,
                                                                       aOptions);
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

  nsTArray<PermissionChoice> emptyChoices;

  unused << ContentPermissionRequestParent::Send__delete__(mParent, false, emptyChoices);
  mParent = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::Allow(JS::HandleValue aChoices)
{
  if (mParent == nullptr) {
    return NS_ERROR_FAILURE;
  }

  // Don't send out the delete message when the managing protocol (PBrowser) is
  // being destroyed and PContentPermissionRequest will soon be.
  if (mParent->IsBeingDestroyed()) {
    return NS_ERROR_FAILURE;
  }

#ifdef MOZ_WIDGET_GONK
  uint32_t len = mPermissionRequests.Length();
  for (uint32_t i = 0; i < len; i++) {
    if (mPermissionRequests[i].type().Equals("audio-capture")) {
      GonkPermissionService::GetInstance()->addGrantInfo(
        "android.permission.RECORD_AUDIO",
        static_cast<TabParent*>(mParent->Manager())->Manager()->Pid());
    }
    if (mPermissionRequests[i].type().Equals("video-capture")) {
      GonkPermissionService::GetInstance()->addGrantInfo(
        "android.permission.CAMERA",
        static_cast<TabParent*>(mParent->Manager())->Manager()->Pid());
    }
  }
#endif

  nsTArray<PermissionChoice> choices;
  if (aChoices.isNullOrUndefined()) {
    // No choice is specified.
  } else if (aChoices.isObject()) {
    // Iterate through all permission types.
    for (uint32_t i = 0; i < mPermissionRequests.Length(); ++i) {
      nsCString type = mPermissionRequests[i].type();

      mozilla::AutoSafeJSContext cx;
      JS::Rooted<JSObject*> obj(cx, &aChoices.toObject());
      JSAutoCompartment ac(cx, obj);

      JS::Rooted<JS::Value> val(cx);

      if (!JS_GetProperty(cx, obj, type.BeginReading(), &val) ||
          !val.isString()) {
        // no setting for the permission type, skip it
      } else {
        nsDependentJSString choice;
        if (!choice.init(cx, val)) {
          return NS_ERROR_FAILURE;
        }
        choices.AppendElement(PermissionChoice(type, choice));
      }
    }
  } else {
    MOZ_ASSERT(false, "SelectedChoices should be undefined or an JS object");
    return NS_ERROR_FAILURE;
  }

  unused << ContentPermissionRequestParent::Send__delete__(mParent, true, choices);
  mParent = nullptr;
  return NS_OK;
}
