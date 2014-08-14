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
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/unused.h"
#include "nsComponentManagerUtils.h"
#include "nsArrayUtils.h"
#include "nsIMutableArray.h"
#include "nsContentPermissionHelper.h"
#include "nsCxPusher.h"
#include "nsJSUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"

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

NS_IMPL_ISUPPORTS(ContentPermissionType, nsIContentPermissionType)

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

// nsContentPermissionUtils

/* static */ uint32_t
nsContentPermissionUtils::ConvertPermissionRequestToArray(nsTArray<PermissionRequest>& aSrcArray,
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

/* static */ uint32_t
nsContentPermissionUtils::ConvertArrayToPermissionRequest(nsIArray* aSrcArray,
                                                          nsTArray<PermissionRequest>& aDesArray)
{
  uint32_t len = 0;
  aSrcArray->GetLength(&len);
  for (uint32_t i = 0; i < len; i++) {
    nsCOMPtr<nsIContentPermissionType> cpt = do_QueryElementAt(aSrcArray, i);
    nsAutoCString type;
    nsAutoCString access;
    cpt->GetType(type);
    cpt->GetAccess(access);

    nsCOMPtr<nsIArray> optionArray;
    cpt->GetOptions(getter_AddRefs(optionArray));
    uint32_t optionsLength = 0;
    if (optionArray) {
      optionArray->GetLength(&optionsLength);
    }
    nsTArray<nsString> options;
    for (uint32_t j = 0; j < optionsLength; ++j) {
      nsCOMPtr<nsISupportsString> isupportsString = do_QueryElementAt(optionArray, j);
      if (isupportsString) {
        nsString option;
        isupportsString->GetData(option);
        options.AppendElement(option);
      }
    }

    aDesArray.AppendElement(PermissionRequest(type, access, options));
  }
  return len;
}


/* static */ nsresult
nsContentPermissionUtils::CreatePermissionArray(const nsACString& aType,
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

/* static */ PContentPermissionRequestParent*
nsContentPermissionUtils::CreateContentPermissionRequestParent(const nsTArray<PermissionRequest>& aRequests,
                                                               Element* element,
                                                               const IPC::Principal& principal)
{
  return new ContentPermissionRequestParent(aRequests, element, principal);
}

/* static */ nsresult
nsContentPermissionUtils::AskPermission(nsIContentPermissionRequest* aRequest, nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(!aWindow || aWindow->IsInnerWindow());
  NS_ENSURE_STATE(aWindow && aWindow->IsCurrentInnerWindow());

  // for content process
  if (XRE_GetProcessType() == GeckoProcessType_Content) {

    nsRefPtr<RemotePermissionRequest> req =
      new RemotePermissionRequest(aRequest, aWindow);

    MOZ_ASSERT(NS_IsMainThread()); // IPC can only be execute on main thread.

    TabChild* child = TabChild::GetFrom(aWindow->GetDocShell());
    NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);

    nsCOMPtr<nsIArray> typeArray;
    nsresult rv = aRequest->GetTypes(getter_AddRefs(typeArray));
    NS_ENSURE_SUCCESS(rv, rv);

    nsTArray<PermissionRequest> permArray;
    ConvertArrayToPermissionRequest(typeArray, permArray);

    nsCOMPtr<nsIPrincipal> principal;
    rv = aRequest->GetPrincipal(getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    req->IPDLAddRef();
    child->SendPContentPermissionRequestConstructor(req,
                                                    permArray,
                                                    IPC::Principal(principal));

    req->Sendprompt();
    return NS_OK;
  }

  // for chrome process
  nsCOMPtr<nsIContentPermissionPrompt> prompt =
    do_GetService(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
  if (prompt) {
    prompt->Prompt(aRequest);
  }
  return NS_OK;
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
  mParent = nullptr;
}

NS_IMPL_ISUPPORTS(nsContentPermissionRequestProxy, nsIContentPermissionRequest)

NS_IMETHODIMP
nsContentPermissionRequestProxy::GetTypes(nsIArray** aTypes)
{
  nsCOMPtr<nsIMutableArray> types = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (mozilla::dom::nsContentPermissionUtils::ConvertPermissionRequestToArray(mPermissionRequests, types)) {
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
    if (mPermissionRequests[i].type().EqualsLiteral("audio-capture")) {
      GonkPermissionService::GetInstance()->addGrantInfo(
        "android.permission.RECORD_AUDIO",
        static_cast<TabParent*>(
          mParent->Manager())->Manager()->AsContentParent()->Pid());
    }
    if (mPermissionRequests[i].type().EqualsLiteral("video-capture")) {
      GonkPermissionService::GetInstance()->addGrantInfo(
        "android.permission.CAMERA",
        static_cast<TabParent*>(
          mParent->Manager())->Manager()->AsContentParent()->Pid());
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
        nsAutoJSString choice;
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

// RemotePermissionRequest

NS_IMPL_ISUPPORTS0(RemotePermissionRequest)

RemotePermissionRequest::RemotePermissionRequest(
  nsIContentPermissionRequest* aRequest,
  nsPIDOMWindow* aWindow)
  : mRequest(aRequest)
  , mWindow(aWindow)
  , mIPCOpen(false)
{
}

void
RemotePermissionRequest::DoCancel()
{
  NS_ASSERTION(mRequest, "We need a request");
  mRequest->Cancel();
}

void
RemotePermissionRequest::DoAllow(JS::HandleValue aChoices)
{
  NS_ASSERTION(mRequest, "We need a request");
  mRequest->Allow(aChoices);
}

// PContentPermissionRequestChild
bool
RemotePermissionRequest::Recv__delete__(const bool& aAllow,
                                        const nsTArray<PermissionChoice>& aChoices)
{
  if (aAllow && mWindow->IsCurrentInnerWindow()) {
    // Use 'undefined' if no choice is provided.
    if (aChoices.IsEmpty()) {
      DoAllow(JS::UndefinedHandleValue);
      return true;
    }

    // Convert choices to a JS val if any.
    // {"type1": "choice1", "type2": "choiceA"}
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(mWindow))) {
      return true; // This is not an IPC error.
    }

    JSContext* cx = jsapi.cx();
    JS::Rooted<JSObject*> obj(cx);
    obj = JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr());
    for (uint32_t i = 0; i < aChoices.Length(); ++i) {
      const nsString& choice = aChoices[i].choice();
      const nsCString& type = aChoices[i].type();
      JS::Rooted<JSString*> jChoice(cx, JS_NewUCStringCopyN(cx, choice.get(), choice.Length()));
      JS::Rooted<JS::Value> vChoice(cx, StringValue(jChoice));
      if (!JS_SetProperty(cx, obj, type.get(), vChoice)) {
        return false;
      }
    }
    JS::RootedValue val(cx, JS::ObjectValue(*obj));
    DoAllow(val);
  } else {
    DoCancel();
  }
  return true;
}
