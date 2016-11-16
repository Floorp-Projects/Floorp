/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <map>
#ifdef MOZ_WIDGET_GONK
#include "GonkPermission.h"
#endif // MOZ_WIDGET_GONK
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/PContentPermission.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/PContentPermissionRequestParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/Unused.h"
#include "nsComponentManagerUtils.h"
#include "nsArrayUtils.h"
#include "nsIMutableArray.h"
#include "nsContentPermissionHelper.h"
#include "nsJSUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsWeakPtr.h"
#include "ScriptSettings.h"

using mozilla::Unused;          // <snicker>
using namespace mozilla::dom;
using namespace mozilla;

#define kVisibilityChange "visibilitychange"

class VisibilityChangeListener final : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  explicit VisibilityChangeListener(nsPIDOMWindowInner* aWindow);

  void RemoveListener();
  void SetCallback(nsIContentPermissionRequestCallback* aCallback);
  already_AddRefed<nsIContentPermissionRequestCallback> GetCallback();

private:
  virtual ~VisibilityChangeListener() {}

  nsWeakPtr mWindow;
  nsCOMPtr<nsIContentPermissionRequestCallback> mCallback;
};

NS_IMPL_ISUPPORTS(VisibilityChangeListener, nsIDOMEventListener)

VisibilityChangeListener::VisibilityChangeListener(nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(aWindow);

  mWindow = do_GetWeakReference(aWindow);
  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
  if (doc) {
    doc->AddSystemEventListener(NS_LITERAL_STRING(kVisibilityChange),
                                /* listener */ this,
                                /* use capture */ true,
                                /* wants untrusted */ false);
  }

}

NS_IMETHODIMP
VisibilityChangeListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);
  if (!type.EqualsLiteral(kVisibilityChange)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc =
    do_QueryInterface(aEvent->InternalDOMEvent()->GetTarget());
  MOZ_ASSERT(doc);

  if (mCallback) {
    mCallback->NotifyVisibility(!doc->Hidden());
  }

  return NS_OK;
}

void
VisibilityChangeListener::RemoveListener()
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mWindow);
  if (!window) {
    return;
  }

  nsCOMPtr<EventTarget> target = do_QueryInterface(window->GetExtantDoc());
  if (target) {
    target->RemoveSystemEventListener(NS_LITERAL_STRING(kVisibilityChange),
                                      /* listener */ this,
                                      /* use capture */ true);
  }
}

void
VisibilityChangeListener::SetCallback(nsIContentPermissionRequestCallback *aCallback)
{
  mCallback = aCallback;
}

already_AddRefed<nsIContentPermissionRequestCallback>
VisibilityChangeListener::GetCallback()
{
  nsCOMPtr<nsIContentPermissionRequestCallback> callback = mCallback;
  return callback.forget();
}

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
  RefPtr<nsContentPermissionRequestProxy> mProxy;
  nsTArray<PermissionRequest> mRequests;

 private:
  virtual mozilla::ipc::IPCResult Recvprompt();
  virtual mozilla::ipc::IPCResult RecvNotifyVisibility(const bool& aIsVisible);
  virtual mozilla::ipc::IPCResult RecvDestroy();
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

mozilla::ipc::IPCResult
ContentPermissionRequestParent::Recvprompt()
{
  mProxy = new nsContentPermissionRequestProxy();
  if (NS_FAILED(mProxy->Init(mRequests, this))) {
    mProxy->Cancel();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentPermissionRequestParent::RecvNotifyVisibility(const bool& aIsVisible)
{
  if (!mProxy) {
    return IPC_FAIL_NO_REASON(this);
  }
  mProxy->NotifyVisibility(aIsVisible);
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentPermissionRequestParent::RecvDestroy()
{
  Unused << PContentPermissionRequestParent::Send__delete__(this);
  return IPC_OK();
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
  // When ContentParent::MarkAsDead() is called, we are being destroyed.
  // It's unsafe to send out any message now.
  ContentParent* contentParent = static_cast<ContentParent*>(Manager());
  return !contentParent->IsAlive();
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

  options.forget(aOptions);
  return NS_OK;
}

// nsContentPermissionUtils

/* static */ uint32_t
nsContentPermissionUtils::ConvertPermissionRequestToArray(nsTArray<PermissionRequest>& aSrcArray,
                                                          nsIMutableArray* aDesArray)
{
  uint32_t len = aSrcArray.Length();
  for (uint32_t i = 0; i < len; i++) {
    RefPtr<ContentPermissionType> cpt =
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

static std::map<PContentPermissionRequestParent*, TabId>&
ContentPermissionRequestParentMap()
{
  MOZ_ASSERT(NS_IsMainThread());
  static std::map<PContentPermissionRequestParent*, TabId> sPermissionRequestParentMap;
  return sPermissionRequestParentMap;
}

static std::map<PContentPermissionRequestChild*, TabId>&
ContentPermissionRequestChildMap()
{
  MOZ_ASSERT(NS_IsMainThread());
  static std::map<PContentPermissionRequestChild*, TabId> sPermissionRequestChildMap;
  return sPermissionRequestChildMap;
}

/* static */ nsresult
nsContentPermissionUtils::CreatePermissionArray(const nsACString& aType,
                                                const nsACString& aAccess,
                                                const nsTArray<nsString>& aOptions,
                                                nsIArray** aTypesArray)
{
  nsCOMPtr<nsIMutableArray> types = do_CreateInstance(NS_ARRAY_CONTRACTID);
  RefPtr<ContentPermissionType> permType = new ContentPermissionType(aType,
                                                                       aAccess,
                                                                       aOptions);
  types->AppendElement(permType, false);
  types.forget(aTypesArray);

  return NS_OK;
}

/* static */ PContentPermissionRequestParent*
nsContentPermissionUtils::CreateContentPermissionRequestParent(const nsTArray<PermissionRequest>& aRequests,
                                                               Element* element,
                                                               const IPC::Principal& principal,
                                                               const TabId& aTabId)
{
  PContentPermissionRequestParent* parent =
    new ContentPermissionRequestParent(aRequests, element, principal);
  ContentPermissionRequestParentMap()[parent] = aTabId;

  return parent;
}

/* static */ nsresult
nsContentPermissionUtils::AskPermission(nsIContentPermissionRequest* aRequest,
                                        nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(!aWindow || aWindow->IsInnerWindow());
  NS_ENSURE_STATE(aWindow && aWindow->IsCurrentInnerWindow());

  // for content process
  if (XRE_IsContentProcess()) {

    RefPtr<RemotePermissionRequest> req =
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
    ContentChild::GetSingleton()->SendPContentPermissionRequestConstructor(
      req,
      permArray,
      IPC::Principal(principal),
      child->GetTabId());
    ContentPermissionRequestChildMap()[req.get()] = child->GetTabId();

    req->Sendprompt();
    return NS_OK;
  }

  // for chrome process
  nsCOMPtr<nsIContentPermissionPrompt> prompt =
    do_GetService(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
  if (prompt) {
    if (NS_FAILED(prompt->Prompt(aRequest))) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

/* static */ nsTArray<PContentPermissionRequestParent*>
nsContentPermissionUtils::GetContentPermissionRequestParentById(const TabId& aTabId)
{
  nsTArray<PContentPermissionRequestParent*> parentArray;
  for (auto& it : ContentPermissionRequestParentMap()) {
    if (it.second == aTabId) {
      parentArray.AppendElement(it.first);
    }
  }

  return Move(parentArray);
}

/* static */ void
nsContentPermissionUtils::NotifyRemoveContentPermissionRequestParent(
  PContentPermissionRequestParent* aParent)
{
  auto it = ContentPermissionRequestParentMap().find(aParent);
  MOZ_ASSERT(it != ContentPermissionRequestParentMap().end());

  ContentPermissionRequestParentMap().erase(it);
}

/* static */ nsTArray<PContentPermissionRequestChild*>
nsContentPermissionUtils::GetContentPermissionRequestChildById(const TabId& aTabId)
{
  nsTArray<PContentPermissionRequestChild*> childArray;
  for (auto& it : ContentPermissionRequestChildMap()) {
    if (it.second == aTabId) {
      childArray.AppendElement(it.first);
    }
  }

  return Move(childArray);
}

/* static */ void
nsContentPermissionUtils::NotifyRemoveContentPermissionRequestChild(
  PContentPermissionRequestChild* aChild)
{
  auto it = ContentPermissionRequestChildMap().find(aChild);
  MOZ_ASSERT(it != ContentPermissionRequestChildMap().end());

  ContentPermissionRequestChildMap().erase(it);
}

NS_IMPL_ISUPPORTS(nsContentPermissionRequester, nsIContentPermissionRequester)

nsContentPermissionRequester::nsContentPermissionRequester(nsPIDOMWindowInner* aWindow)
  : mWindow(do_GetWeakReference(aWindow))
  , mListener(new VisibilityChangeListener(aWindow))
{
}

nsContentPermissionRequester::~nsContentPermissionRequester()
{
  mListener->RemoveListener();
  mListener = nullptr;
}

NS_IMETHODIMP
nsContentPermissionRequester::GetVisibility(nsIContentPermissionRequestCallback* aCallback)
{
  NS_ENSURE_ARG_POINTER(aCallback);

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mWindow);
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docshell = window->GetDocShell();
  if (!docshell) {
    return NS_ERROR_FAILURE;
  }

  bool isActive = false;
  docshell->GetIsActive(&isActive);
  aCallback->NotifyVisibility(isActive);
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequester::SetOnVisibilityChange(nsIContentPermissionRequestCallback* aCallback)
{
  mListener->SetCallback(aCallback);

  if (!aCallback) {
    mListener->RemoveListener();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequester::GetOnVisibilityChange(nsIContentPermissionRequestCallback** aCallback)
{
  NS_ENSURE_ARG_POINTER(aCallback);

  nsCOMPtr<nsIContentPermissionRequestCallback> callback = mListener->GetCallback();
  callback.forget(aCallback);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

NS_IMPL_ISUPPORTS(nsContentPermissionRequestProxy::nsContentPermissionRequesterProxy,
                  nsIContentPermissionRequester)

NS_IMETHODIMP
nsContentPermissionRequestProxy::nsContentPermissionRequesterProxy
  ::GetVisibility(nsIContentPermissionRequestCallback* aCallback)
{
  NS_ENSURE_ARG_POINTER(aCallback);

  mGetCallback = aCallback;
  mWaitGettingResult = true;
  Unused << mParent->SendGetVisibility();
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::nsContentPermissionRequesterProxy
  ::SetOnVisibilityChange(nsIContentPermissionRequestCallback* aCallback)
{
  mOnChangeCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::nsContentPermissionRequesterProxy
  ::GetOnVisibilityChange(nsIContentPermissionRequestCallback** aCallback)
{
  NS_ENSURE_ARG_POINTER(aCallback);

  nsCOMPtr<nsIContentPermissionRequestCallback> callback = mOnChangeCallback;
  callback.forget(aCallback);
  return NS_OK;
}

void
nsContentPermissionRequestProxy::nsContentPermissionRequesterProxy
  ::NotifyVisibilityResult(const bool& aIsVisible)
{
  if (mWaitGettingResult) {
    MOZ_ASSERT(mGetCallback);
    mWaitGettingResult = false;
    mGetCallback->NotifyVisibility(aIsVisible);
    return;
  }

  if (mOnChangeCallback) {
    mOnChangeCallback->NotifyVisibility(aIsVisible);
  }
}

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
  mRequester = new nsContentPermissionRequesterProxy(mParent);

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
  mRequester = nullptr;
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
nsContentPermissionRequestProxy::GetWindow(mozIDOMWindow * *aRequestingWindow)
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

  Unused << mParent->SendNotifyResult(false, emptyChoices);
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

  nsTArray<PermissionChoice> choices;
  if (aChoices.isNullOrUndefined()) {
    // No choice is specified.
  } else if (aChoices.isObject()) {
    // Iterate through all permission types.
    for (uint32_t i = 0; i < mPermissionRequests.Length(); ++i) {
      nsCString type = mPermissionRequests[i].type();

      AutoJSAPI jsapi;
      jsapi.Init();

      JSContext* cx = jsapi.cx();
      JS::Rooted<JSObject*> obj(cx, &aChoices.toObject());
      JSAutoCompartment ac(cx, obj);

      JS::Rooted<JS::Value> val(cx);

      if (!JS_GetProperty(cx, obj, type.BeginReading(), &val) ||
          !val.isString()) {
        // no setting for the permission type, clear exception and skip it
        jsapi.ClearException();
      } else {
        nsAutoJSString choice;
        if (!choice.init(cx, val)) {
          jsapi.ClearException();
          return NS_ERROR_FAILURE;
        }
        choices.AppendElement(PermissionChoice(type, choice));
      }
    }
  } else {
    MOZ_ASSERT(false, "SelectedChoices should be undefined or an JS object");
    return NS_ERROR_FAILURE;
  }

  Unused << mParent->SendNotifyResult(true, choices);
  mParent = nullptr;
  return NS_OK;
}

void
nsContentPermissionRequestProxy::NotifyVisibility(const bool& aIsVisible)
{
  MOZ_ASSERT(mRequester);

  mRequester->NotifyVisibilityResult(aIsVisible);
}

NS_IMETHODIMP
nsContentPermissionRequestProxy::GetRequester(nsIContentPermissionRequester** aRequester)
{
  NS_ENSURE_ARG_POINTER(aRequester);

  RefPtr<nsContentPermissionRequesterProxy> requester = mRequester;
  requester.forget(aRequester);
  return NS_OK;
}

// RemotePermissionRequest

NS_IMPL_ISUPPORTS(RemotePermissionRequest, nsIContentPermissionRequestCallback);

RemotePermissionRequest::RemotePermissionRequest(
  nsIContentPermissionRequest* aRequest,
  nsPIDOMWindowInner* aWindow)
  : mRequest(aRequest)
  , mWindow(aWindow)
  , mIPCOpen(false)
  , mDestroyed(false)
{
  mListener = new VisibilityChangeListener(mWindow);
  mListener->SetCallback(this);
}

RemotePermissionRequest::~RemotePermissionRequest()
{
  MOZ_ASSERT(!mIPCOpen, "Protocol must not be open when RemotePermissionRequest is destroyed.");
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
mozilla::ipc::IPCResult
RemotePermissionRequest::RecvNotifyResult(const bool& aAllow,
                                          InfallibleTArray<PermissionChoice>&& aChoices)
{
  Destroy();

  if (aAllow && mWindow->IsCurrentInnerWindow()) {
    // Use 'undefined' if no choice is provided.
    if (aChoices.IsEmpty()) {
      DoAllow(JS::UndefinedHandleValue);
      return IPC_OK();
    }

    // Convert choices to a JS val if any.
    // {"type1": "choice1", "type2": "choiceA"}
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(mWindow))) {
      return IPC_OK(); // This is not an IPC error.
    }

    JSContext* cx = jsapi.cx();
    JS::Rooted<JSObject*> obj(cx);
    obj = JS_NewPlainObject(cx);
    for (uint32_t i = 0; i < aChoices.Length(); ++i) {
      const nsString& choice = aChoices[i].choice();
      const nsCString& type = aChoices[i].type();
      JS::Rooted<JSString*> jChoice(cx, JS_NewUCStringCopyN(cx, choice.get(), choice.Length()));
      JS::Rooted<JS::Value> vChoice(cx, StringValue(jChoice));
      if (!JS_SetProperty(cx, obj, type.get(), vChoice)) {
        return IPC_FAIL_NO_REASON(this);
      }
    }
    JS::RootedValue val(cx, JS::ObjectValue(*obj));
    DoAllow(val);
  } else {
    DoCancel();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
RemotePermissionRequest::RecvGetVisibility()
{
  nsCOMPtr<nsIDocShell> docshell = mWindow->GetDocShell();
  if (!docshell) {
    return IPC_FAIL_NO_REASON(this);
  }

  bool isActive = false;
  docshell->GetIsActive(&isActive);
  Unused << SendNotifyVisibility(isActive);
  return IPC_OK();
}

void
RemotePermissionRequest::Destroy()
{
  if (!IPCOpen()) {
    return;
  }
  Unused << this->SendDestroy();
  mListener->RemoveListener();
  mListener = nullptr;
  mDestroyed = true;
}

NS_IMETHODIMP
RemotePermissionRequest::NotifyVisibility(bool isVisible)
{
  if (!IPCOpen()) {
    return NS_OK;
  }

  Unused << SendNotifyVisibility(isVisible);
  return NS_OK;
}
