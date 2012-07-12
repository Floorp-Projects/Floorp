/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TabParent.h"

// TabParent.h transitively includes <windows.h>, which does
//   #define CreateEvent CreateEventW
// That messes up our call to nsEventDispatcher::CreateEvent below.

#ifdef CreateEvent
#undef CreateEvent
#endif

#include "BrowserElementParent.h"
#include "nsHTMLIFrameElement.h"
#include "nsOpenWindowEventDetail.h"
#include "nsEventDispatcher.h"
#include "nsIDOMCustomEvent.h"
#include "nsVariant.h"

using mozilla::dom::Element;
using mozilla::dom::TabParent;

namespace {

/**
 * Create an <iframe mozbrowser> owned by the same document as
 * aOpenerFrameElement.
 */
already_AddRefed<nsHTMLIFrameElement>
CreateIframe(Element* aOpenerFrameElement)
{
  nsNodeInfoManager *nodeInfoManager =
    aOpenerFrameElement->OwnerDoc()->NodeInfoManager();

  nsCOMPtr<nsINodeInfo> nodeInfo =
    nodeInfoManager->GetNodeInfo(nsGkAtoms::iframe,
                                 /* aPrefix = */ nsnull,
                                 kNameSpaceID_XHTML,
                                 nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<nsHTMLIFrameElement> popupFrameElement =
    static_cast<nsHTMLIFrameElement*>(
      NS_NewHTMLIFrameElement(nodeInfo.forget(), mozilla::dom::NOT_FROM_PARSER));

  popupFrameElement->SetMozbrowser(true);

  // Copy the opener frame's mozapp attribute to the popup frame.
  if (aOpenerFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::mozapp)) {
    nsAutoString mozapp;
    aOpenerFrameElement->GetAttr(kNameSpaceID_None, nsGkAtoms::mozapp, mozapp);
    popupFrameElement->SetAttr(kNameSpaceID_None, nsGkAtoms::mozapp,
                               mozapp, /* aNotify = */ false);
  }

  return popupFrameElement.forget();
}

/**
 * Dispatch a mozbrowseropenwindow event to the given opener frame element.
 * The "popup iframe" (event.detail.frameElement) will be |aPopupFrameElement|.
 *
 * Returns true iff there were no unexpected failures and the window.open call
 * was accepted by the embedder.
 */
bool
DispatchOpenWindowEvent(Element* aOpenerFrameElement,
                        Element* aPopupFrameElement,
                        const nsAString& aURL,
                        const nsAString& aName,
                        const nsAString& aFeatures)
{
  // Dispatch a CustomEvent at aOpenerFrameElement with a detail object
  // (nsIOpenWindowEventDetail) containing aPopupFrameElement, aURL, aName, and
  // aFeatures.

  // Create the event's detail object.
  nsRefPtr<nsOpenWindowEventDetail> detail =
    new nsOpenWindowEventDetail(aURL, aName, aFeatures,
                                aPopupFrameElement->AsDOMNode());
  nsCOMPtr<nsIWritableVariant> detailVariant = new nsVariant();
  nsresult rv = detailVariant->SetAsISupports(detail);
  NS_ENSURE_SUCCESS(rv, false);

  // Create the CustomEvent.
  nsIPresShell *shell = aOpenerFrameElement->OwnerDoc()->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell) {
    presContext = shell->GetPresContext();
  }

  nsCOMPtr<nsIDOMEvent> domEvent;
  nsEventDispatcher::CreateEvent(presContext, nsnull,
                                 NS_LITERAL_STRING("customevent"),
                                 getter_AddRefs(domEvent));
  NS_ENSURE_TRUE(domEvent, false);

  nsCOMPtr<nsIDOMCustomEvent> customEvent = do_QueryInterface(domEvent);
  NS_ENSURE_TRUE(customEvent, false);
  customEvent->InitCustomEvent(NS_LITERAL_STRING("mozbrowseropenwindow"),
                               /* bubbles = */ true,
                               /* cancelable = */ false,
                               detailVariant);
  customEvent->SetTrusted(true);

  // Dispatch the event.
  nsEventStatus status = nsEventStatus_eIgnore;
  rv = nsEventDispatcher::DispatchDOMEvent(aOpenerFrameElement, nsnull,
                                           domEvent, presContext, &status);
  NS_ENSURE_SUCCESS(rv, false);

  // If the iframe is not in some document's DOM at this point, the embedder
  // has "blocked" the popup.
  return aPopupFrameElement->IsInDoc();
}

} // anonymous namespace

namespace mozilla {

/*static*/ bool
BrowserElementParent::OpenWindowOOP(mozilla::dom::TabParent* aOpenerTabParent,
                                    mozilla::dom::TabParent* aPopupTabParent,
                                    const nsAString& aURL,
                                    const nsAString& aName,
                                    const nsAString& aFeatures)
{
  // Create an iframe owned by the same document which owns openerFrameElement.
  nsCOMPtr<Element> openerFrameElement =
    do_QueryInterface(aOpenerTabParent->GetOwnerElement());
  NS_ENSURE_TRUE(openerFrameElement, false);
  nsRefPtr<nsHTMLIFrameElement> popupFrameElement =
    CreateIframe(openerFrameElement);

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

  bool dispatchSucceeded =
    DispatchOpenWindowEvent(openerFrameElement, popupFrameElement,
                            aURL, aName, aFeatures);
  if (!dispatchSucceeded) {
    return false;
  }

  // The popup was not blocked, so hook up the frame element and the popup tab
  // parent, and return success.
  aPopupTabParent->SetOwnerElement(popupFrameElement);
  popupFrameElement->AllowCreateFrameLoader();
  popupFrameElement->CreateRemoteFrameLoader(aPopupTabParent);

  return true;
}

/* static */ bool
BrowserElementParent::OpenWindowInProcess(nsIDOMWindow* aOpenerWindow,
                                          nsIURI* aURI,
                                          const nsAString& aName,
                                          const nsACString& aFeatures,
                                          nsIDOMWindow** aReturnWindow)
{
  *aReturnWindow = NULL;

  // If we call window.open from an <iframe> inside an <iframe mozbrowser>,
  // it's as though the top-level document inside the <iframe mozbrowser>
  // called window.open.  (Indeed, in the OOP case, the inner <iframe> lives
  // out-of-process, so we couldn't touch it if we tried.)
  //
  // GetScriptableTop gets us the <iframe mozbrowser>'s window; we'll use its
  // frame element, rather than aOpenerWindow's frame element, as our "opener
  // frame element" below.
  nsCOMPtr<nsIDOMWindow> topWindow;
  aOpenerWindow->GetScriptableTop(getter_AddRefs(topWindow));

  nsCOMPtr<nsIDOMElement> openerFrameDOMElement;
  topWindow->GetFrameElement(getter_AddRefs(openerFrameDOMElement));
  NS_ENSURE_TRUE(openerFrameDOMElement, false);

  nsCOMPtr<Element> openerFrameElement =
    do_QueryInterface(openerFrameDOMElement);

  nsRefPtr<nsHTMLIFrameElement> popupFrameElement =
    CreateIframe(openerFrameElement);
  NS_ENSURE_TRUE(popupFrameElement, false);

  nsCAutoString spec;
  aURI->GetSpec(spec);
  bool dispatchSucceeded =
    DispatchOpenWindowEvent(openerFrameElement, popupFrameElement,
                            NS_ConvertUTF8toUTF16(spec),
                            aName,
                            NS_ConvertUTF8toUTF16(aFeatures));
  if (!dispatchSucceeded) {
    return false;
  }

  // Return popupFrameElement's window.
  nsCOMPtr<nsIFrameLoader> frameLoader;
  popupFrameElement->GetFrameLoader(getter_AddRefs(frameLoader));
  NS_ENSURE_TRUE(frameLoader, false);

  nsCOMPtr<nsIDocShell> docshell;
  frameLoader->GetDocShell(getter_AddRefs(docshell));
  NS_ENSURE_TRUE(docshell, false);

  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(docshell);
  window.forget(aReturnWindow);
  return !!*aReturnWindow;
}

} // namespace mozilla
