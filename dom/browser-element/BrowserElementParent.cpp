/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TabParent.h"

// TabParent.h transitively includes <windows.h>, which does
//   #define CreateEvent CreateEventW
// That messes up our call to EventDispatcher::CreateEvent below.

#ifdef CreateEvent
#undef CreateEvent
#endif

#include "BrowserElementParent.h"
#include "BrowserElementAudioChannel.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsVariant.h"
#include "mozilla/dom/BrowserElementDictionariesBinding.h"
#include "mozilla/dom/CustomEvent.h"
#include "mozilla/layout/RenderFrameParent.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::layout;

namespace {

using mozilla::BrowserElementParent;
/**
 * Create an <iframe mozbrowser> owned by the same document as
 * aOpenerFrameElement.
 */
already_AddRefed<HTMLIFrameElement>
CreateIframe(Element* aOpenerFrameElement, const nsAString& aName, bool aRemote)
{
  nsNodeInfoManager *nodeInfoManager =
    aOpenerFrameElement->OwnerDoc()->NodeInfoManager();

  RefPtr<NodeInfo> nodeInfo =
    nodeInfoManager->GetNodeInfo(nsGkAtoms::iframe,
                                 /* aPrefix = */ nullptr,
                                 kNameSpaceID_XHTML,
                                 nsIDOMNode::ELEMENT_NODE);

  RefPtr<HTMLIFrameElement> popupFrameElement =
    static_cast<HTMLIFrameElement*>(
      NS_NewHTMLIFrameElement(nodeInfo.forget(), mozilla::dom::NOT_FROM_PARSER));

  popupFrameElement->SetMozbrowser(true);

  // Copy the window name onto the iframe.
  popupFrameElement->SetAttr(kNameSpaceID_None, nsGkAtoms::name,
                             aName, /* aNotify = */ false);

  // Indicate whether the iframe is should be remote.
  popupFrameElement->SetAttr(kNameSpaceID_None, nsGkAtoms::Remote,
                             aRemote ? NS_LITERAL_STRING("true") :
                                       NS_LITERAL_STRING("false"),
                             /* aNotify = */ false);

  // Copy the opener frame's mozprivatebrowsing attribute to the popup frame.
  nsAutoString mozprivatebrowsing;
  if (aOpenerFrameElement->GetAttr(kNameSpaceID_None, nsGkAtoms::mozprivatebrowsing,
                                   mozprivatebrowsing)) {
    popupFrameElement->SetAttr(kNameSpaceID_None, nsGkAtoms::mozprivatebrowsing,
                               mozprivatebrowsing, /* aNotify = */ false);
  }

  return popupFrameElement.forget();
}

bool
DispatchCustomDOMEvent(Element* aFrameElement, const nsAString& aEventName,
                       JSContext* cx, JS::Handle<JS::Value> aDetailValue,
                       nsEventStatus *aStatus)
{
  NS_ENSURE_TRUE(aFrameElement, false);
  nsIPresShell *shell = aFrameElement->OwnerDoc()->GetShell();
  RefPtr<nsPresContext> presContext;
  if (shell) {
    presContext = shell->GetPresContext();
  }

  RefPtr<CustomEvent> event =
    NS_NewDOMCustomEvent(aFrameElement, presContext, nullptr);

  ErrorResult res;
  event->InitCustomEvent(cx,
                         aEventName,
                         /* aCanBubble = */ true,
                         /* aCancelable = */ true,
                         aDetailValue,
                         res);
  if (res.Failed()) {
    return false;
  }
  event->SetTrusted(true);
  // Dispatch the event.
  // We don't initialize aStatus here, as our callers have already done so.
  nsresult rv =
    EventDispatcher::DispatchDOMEvent(aFrameElement, nullptr, event,
                                      presContext, aStatus);
  return NS_SUCCEEDED(rv);
}

} // namespace

namespace mozilla {

/**
 * Dispatch a mozbrowseropenwindow event to the given opener frame element.
 * The "popup iframe" (event.detail.frameElement) will be |aPopupFrameElement|.
 *
 * Returns true iff there were no unexpected failures and the window.open call
 * was accepted by the embedder.
 */
/*static*/
BrowserElementParent::OpenWindowResult
BrowserElementParent::DispatchOpenWindowEvent(Element* aOpenerFrameElement,
                        Element* aPopupFrameElement,
                        const nsAString& aURL,
                        const nsAString& aName,
                        const nsAString& aFeatures)
{
  // Dispatch a CustomEvent at aOpenerFrameElement with a detail object
  // (OpenWindowEventDetail) containing aPopupFrameElement, aURL, aName, and
  // aFeatures.

  // Create the event's detail object.
  OpenWindowEventDetail detail;
  if (aURL.IsEmpty()) {
    // URL should never be empty. Assign about:blank as default.
    detail.mUrl = NS_LITERAL_STRING("about:blank");
  } else {
    detail.mUrl = aURL;
  }
  detail.mName = aName;
  detail.mFeatures = aFeatures;
  detail.mFrameElement = aPopupFrameElement;

  AutoJSContext cx;
  JS::Rooted<JS::Value> val(cx);

  nsIGlobalObject* sgo = aPopupFrameElement->OwnerDoc()->GetScopeObject();
  if (!sgo) {
    return BrowserElementParent::OPEN_WINDOW_IGNORED;
  }

  JS::Rooted<JSObject*> global(cx, sgo->GetGlobalJSObject());
  JSAutoCompartment ac(cx, global);
  if (!ToJSValue(cx, detail, &val)) {
    MOZ_CRASH("Failed to convert dictionary to JS::Value due to OOM.");
    return BrowserElementParent::OPEN_WINDOW_IGNORED;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  bool dispatchSucceeded =
    DispatchCustomDOMEvent(aOpenerFrameElement,
                           NS_LITERAL_STRING("mozbrowseropenwindow"),
                           cx,
                           val, &status);

  if (dispatchSucceeded) {
    if (aPopupFrameElement->IsInUncomposedDoc()) {
      return BrowserElementParent::OPEN_WINDOW_ADDED;
    } else if (status == nsEventStatus_eConsumeNoDefault) {
      // If the frame was not added to a document, report to callers whether
      // preventDefault was called on or not
      return BrowserElementParent::OPEN_WINDOW_CANCELLED;
    }
  }

  return BrowserElementParent::OPEN_WINDOW_IGNORED;
}

/*static*/
BrowserElementParent::OpenWindowResult
BrowserElementParent::OpenWindowOOP(TabParent* aOpenerTabParent,
                                    TabParent* aPopupTabParent,
                                    PRenderFrameParent* aRenderFrame,
                                    const nsAString& aURL,
                                    const nsAString& aName,
                                    const nsAString& aFeatures,
                                    TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                    uint64_t* aLayersId)
{
  // Create an iframe owned by the same document which owns openerFrameElement.
  nsCOMPtr<Element> openerFrameElement = aOpenerTabParent->GetOwnerElement();
  NS_ENSURE_TRUE(openerFrameElement,
                 BrowserElementParent::OPEN_WINDOW_IGNORED);
  RefPtr<HTMLIFrameElement> popupFrameElement =
    CreateIframe(openerFrameElement, aName, /* aRemote = */ true);

  // Normally an <iframe> element will try to create a frameLoader when the
  // page touches iframe.contentWindow or sets iframe.src.
  //
  // But in our case, we want to delay the creation of the frameLoader until
  // we've verified that the popup has gone through successfully.  If the popup
  // is "blocked" by the embedder, we don't want to load the popup's url.
  //
  // Therefore we call DisallowCreateFrameLoader() on the element and call
  // AllowCreateFrameLoader() only after we've verified that the popup was
  // allowed.
  popupFrameElement->DisallowCreateFrameLoader();

  OpenWindowResult opened =
    DispatchOpenWindowEvent(openerFrameElement, popupFrameElement,
                            aURL, aName, aFeatures);

  if (opened != BrowserElementParent::OPEN_WINDOW_ADDED) {
    return opened;
  }

  // The popup was not blocked, so hook up the frame element and the popup tab
  // parent, and return success.
  aPopupTabParent->SetOwnerElement(popupFrameElement);
  popupFrameElement->AllowCreateFrameLoader();
  popupFrameElement->CreateRemoteFrameLoader(aPopupTabParent);

  RenderFrameParent* rfp = static_cast<RenderFrameParent*>(aRenderFrame);
  if (!aPopupTabParent->SetRenderFrame(rfp) ||
      !aPopupTabParent->GetRenderFrameInfo(aTextureFactoryIdentifier, aLayersId)) {
    return BrowserElementParent::OPEN_WINDOW_IGNORED;
  }

  return opened;
}

/* static */
BrowserElementParent::OpenWindowResult
BrowserElementParent::OpenWindowInProcess(nsPIDOMWindowOuter* aOpenerWindow,
                                          nsIURI* aURI,
                                          const nsAString& aName,
                                          const nsACString& aFeatures,
                                          bool aForceNoOpener,
                                          mozIDOMWindowProxy** aReturnWindow)
{
  *aReturnWindow = nullptr;

  // If we call window.open from an <iframe> inside an <iframe mozbrowser>,
  // it's as though the top-level document inside the <iframe mozbrowser>
  // called window.open.  (Indeed, in the OOP case, the inner <iframe> lives
  // out-of-process, so we couldn't touch it if we tried.)
  //
  // GetScriptableTop gets us the <iframe mozbrowser>'s window; we'll use its
  // frame element, rather than aOpenerWindow's frame element, as our "opener
  // frame element" below.
  nsCOMPtr<nsPIDOMWindowOuter> win = aOpenerWindow->GetScriptableTop();

  nsCOMPtr<Element> openerFrameElement = win->GetFrameElementInternal();
  NS_ENSURE_TRUE(openerFrameElement, BrowserElementParent::OPEN_WINDOW_IGNORED);


  RefPtr<HTMLIFrameElement> popupFrameElement =
    CreateIframe(openerFrameElement, aName, /* aRemote = */ false);
  NS_ENSURE_TRUE(popupFrameElement, BrowserElementParent::OPEN_WINDOW_IGNORED);

  nsAutoCString spec;
  if (aURI) {
    aURI->GetSpec(spec);
  }

  if (!aForceNoOpener) {
    ErrorResult res;
    popupFrameElement->PresetOpenerWindow(aOpenerWindow, res);
    MOZ_ASSERT(!res.Failed());
  }

  OpenWindowResult opened =
    DispatchOpenWindowEvent(openerFrameElement, popupFrameElement,
                            NS_ConvertUTF8toUTF16(spec),
                            aName,
                            NS_ConvertUTF8toUTF16(aFeatures));

  if (opened != BrowserElementParent::OPEN_WINDOW_ADDED) {
    return opened;
  }

  // Return popupFrameElement's window.
  RefPtr<nsFrameLoader> frameLoader = popupFrameElement->GetFrameLoader();
  NS_ENSURE_TRUE(frameLoader, BrowserElementParent::OPEN_WINDOW_IGNORED);

  nsCOMPtr<nsIDocShell> docshell;
  frameLoader->GetDocShell(getter_AddRefs(docshell));
  NS_ENSURE_TRUE(docshell, BrowserElementParent::OPEN_WINDOW_IGNORED);

  nsCOMPtr<nsPIDOMWindowOuter> window = docshell->GetWindow();
  window.forget(aReturnWindow);

  return !!*aReturnWindow ? opened : BrowserElementParent::OPEN_WINDOW_CANCELLED;
}

} // namespace mozilla
