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
#include "mozilla/dom/HTMLIFrameElement.h"
#include "nsOpenWindowEventDetail.h"
#include "nsEventDispatcher.h"
#include "nsIDOMCustomEvent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsVariant.h"
#include "nsAsyncScrollEventDetail.h"

using mozilla::dom::Element;
using mozilla::dom::HTMLIFrameElement;
using mozilla::dom::TabParent;

namespace {

/**
 * Create an <iframe mozbrowser> owned by the same document as
 * aOpenerFrameElement.
 */
already_AddRefed<HTMLIFrameElement>
CreateIframe(Element* aOpenerFrameElement, const nsAString& aName, bool aRemote)
{
  nsNodeInfoManager *nodeInfoManager =
    aOpenerFrameElement->OwnerDoc()->NodeInfoManager();

  nsCOMPtr<nsINodeInfo> nodeInfo =
    nodeInfoManager->GetNodeInfo(nsGkAtoms::iframe,
                                 /* aPrefix = */ nullptr,
                                 kNameSpaceID_XHTML,
                                 nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<HTMLIFrameElement> popupFrameElement =
    static_cast<HTMLIFrameElement*>(
      NS_NewHTMLIFrameElement(nodeInfo.forget(), mozilla::dom::NOT_FROM_PARSER));

  popupFrameElement->SetMozbrowser(true);

  // Copy the opener frame's mozapp attribute to the popup frame.
  if (aOpenerFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::mozapp)) {
    nsAutoString mozapp;
    aOpenerFrameElement->GetAttr(kNameSpaceID_None, nsGkAtoms::mozapp, mozapp);
    popupFrameElement->SetAttr(kNameSpaceID_None, nsGkAtoms::mozapp,
                               mozapp, /* aNotify = */ false);
  }

  // Copy the window name onto the iframe.
  popupFrameElement->SetAttr(kNameSpaceID_None, nsGkAtoms::name,
                             aName, /* aNotify = */ false);

  // Indicate whether the iframe is should be remote.
  popupFrameElement->SetAttr(kNameSpaceID_None, nsGkAtoms::Remote,
                             aRemote ? NS_LITERAL_STRING("true") :
                                       NS_LITERAL_STRING("false"),
                             /* aNotify = */ false);

  return popupFrameElement.forget();
}

bool
DispatchCustomDOMEvent(Element* aFrameElement, const nsAString& aEventName,
                       nsISupports *aDetailValue)
{
  NS_ENSURE_TRUE(aFrameElement, false);
  nsIPresShell *shell = aFrameElement->OwnerDoc()->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell) {
    presContext = shell->GetPresContext();
  }

  nsCOMPtr<nsIDOMEvent> domEvent;
  nsEventDispatcher::CreateEvent(aFrameElement, presContext, nullptr,
                                 NS_LITERAL_STRING("customevent"),
                                 getter_AddRefs(domEvent));
  NS_ENSURE_TRUE(domEvent, false);

  nsCOMPtr<nsIWritableVariant> detailVariant = new nsVariant();
  nsresult rv = detailVariant->SetAsISupports(aDetailValue);
  NS_ENSURE_SUCCESS(rv, false);
  nsCOMPtr<nsIDOMCustomEvent> customEvent = do_QueryInterface(domEvent);
  NS_ENSURE_TRUE(customEvent, false);
  customEvent->InitCustomEvent(aEventName,
                               /* bubbles = */ true,
                               /* cancelable = */ false,
                               detailVariant);
  customEvent->SetTrusted(true);
  // Dispatch the event.
  nsEventStatus status = nsEventStatus_eIgnore;
  rv = nsEventDispatcher::DispatchDOMEvent(aFrameElement, nullptr,
                                           domEvent, presContext, &status);
  return NS_SUCCEEDED(rv);
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

  bool dispatchSucceeded =
    DispatchCustomDOMEvent(aOpenerFrameElement,
                           NS_LITERAL_STRING("mozbrowseropenwindow"),
                           detail);

  // If the iframe is not in some document's DOM at this point, the embedder
  // has "blocked" the popup.
  return (dispatchSucceeded && aPopupFrameElement->IsInDoc());
}

} // anonymous namespace

namespace mozilla {

/*static*/ bool
BrowserElementParent::OpenWindowOOP(TabParent* aOpenerTabParent,
                                    TabParent* aPopupTabParent,
                                    const nsAString& aURL,
                                    const nsAString& aName,
                                    const nsAString& aFeatures)
{
  // Create an iframe owned by the same document which owns openerFrameElement.
  nsCOMPtr<Element> openerFrameElement =
    do_QueryInterface(aOpenerTabParent->GetOwnerElement());
  NS_ENSURE_TRUE(openerFrameElement, false);
  nsRefPtr<HTMLIFrameElement> popupFrameElement =
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

  nsRefPtr<HTMLIFrameElement> popupFrameElement =
    CreateIframe(openerFrameElement, aName, /* aRemote = */ false);
  NS_ENSURE_TRUE(popupFrameElement, false);

  nsAutoCString spec;
  if (aURI) {
    aURI->GetSpec(spec);
  }
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

class DispatchAsyncScrollEventRunnable : public nsRunnable
{
public:
  DispatchAsyncScrollEventRunnable(TabParent* aTabParent,
                                   const CSSRect& aContentRect,
                                   const CSSSize& aContentSize)
    : mTabParent(aTabParent)
    , mContentRect(aContentRect)
    , mContentSize(aContentSize)
  {}

  NS_IMETHOD Run();

private:
  nsRefPtr<TabParent> mTabParent;
  const CSSRect mContentRect;
  const CSSSize mContentSize;
};

NS_IMETHODIMP DispatchAsyncScrollEventRunnable::Run()
{
  nsIDOMElement* element = mTabParent->GetOwnerElement();
  nsCOMPtr<Element> frameElement = do_QueryInterface(element);
  // Create the event's detail object.
  nsRefPtr<nsAsyncScrollEventDetail> detail =
    new nsAsyncScrollEventDetail(mContentRect.x, mContentRect.y,
                                 mContentRect.width, mContentRect.height,
                                 mContentSize.width, mContentSize.height);
  DispatchCustomDOMEvent(frameElement,
                         NS_LITERAL_STRING("mozbrowserasyncscroll"),
                         detail);
  return NS_OK;
}

bool
BrowserElementParent::DispatchAsyncScrollEvent(TabParent* aTabParent,
                                               const CSSRect& aContentRect,
                                               const CSSSize& aContentSize)
{
  nsRefPtr<DispatchAsyncScrollEventRunnable> runnable =
    new DispatchAsyncScrollEventRunnable(aTabParent, aContentRect,
                                         aContentSize);
  return NS_SUCCEEDED(NS_DispatchToMainThread(runnable));
}

} // namespace mozilla
