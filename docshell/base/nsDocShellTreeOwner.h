/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellTreeOwner_h__
#define nsDocShellTreeOwner_h__

// Helper Classes
#include "nsCOMPtr.h"
#include "nsString.h"

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDOMEventListener.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "nsITimer.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsITooltipListener.h"
#include "nsITooltipTextProvider.h"
#include "nsCTooltipTextProvider.h"
#include "nsIDroppedLinkHandler.h"

namespace mozilla {
namespace dom {
class Event;
class EventTarget;
} // namespace dom
} // namespace mozilla

class nsWebBrowser;
class ChromeTooltipListener;

// {6D10C180-6888-11d4-952B-0020183BF181}
#define NS_ICDOCSHELLTREEOWNER_IID \
  { 0x6d10c180, 0x6888, 0x11d4, { 0x95, 0x2b, 0x0, 0x20, 0x18, 0x3b, 0xf1, 0x81 } }

// This is a fake 'hidden' interface that nsDocShellTreeOwner implements.
// Classes can QI for this interface to be sure that
// they're dealing with a valid nsDocShellTreeOwner and not some other object
// that implements nsIDocShellTreeOwner.
class nsICDocShellTreeOwner : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICDOCSHELLTREEOWNER_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICDocShellTreeOwner, NS_ICDOCSHELLTREEOWNER_IID)

class nsDocShellTreeOwner final : public nsIDocShellTreeOwner,
                                  public nsIBaseWindow,
                                  public nsIInterfaceRequestor,
                                  public nsIWebProgressListener,
                                  public nsIDOMEventListener,
                                  public nsICDocShellTreeOwner,
                                  public nsSupportsWeakReference
{
  friend class nsWebBrowser;

public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSIBASEWINDOW
  NS_DECL_NSIDOCSHELLTREEOWNER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIWEBPROGRESSLISTENER

protected:
  nsDocShellTreeOwner();
  virtual ~nsDocShellTreeOwner();

  void WebBrowser(nsWebBrowser* aWebBrowser);

  nsWebBrowser* WebBrowser();
  NS_IMETHOD SetTreeOwner(nsIDocShellTreeOwner* aTreeOwner);
  NS_IMETHOD SetWebBrowserChrome(nsIWebBrowserChrome* aWebBrowserChrome);

  NS_IMETHOD AddChromeListeners();
  NS_IMETHOD RemoveChromeListeners();

  void EnsurePrompter();
  void EnsureAuthPrompter();

  void AddToWatcher();
  void RemoveFromWatcher();

  void EnsureContentTreeOwner();

  // These helper functions return the correct instances of the requested
  // interfaces.  If the object passed to SetWebBrowserChrome() implements
  // nsISupportsWeakReference, then these functions call QueryReferent on
  // that object.  Otherwise, they return an addrefed pointer.  If the
  // WebBrowserChrome object doesn't exist, they return nullptr.
  already_AddRefed<nsIWebBrowserChrome> GetWebBrowserChrome();
  already_AddRefed<nsIEmbeddingSiteWindow> GetOwnerWin();
  already_AddRefed<nsIInterfaceRequestor> GetOwnerRequestor();

protected:
  // Weak References
  nsWebBrowser* mWebBrowser;
  nsIDocShellTreeOwner* mTreeOwner;
  nsIDocShellTreeItem* mPrimaryContentShell;

  nsIWebBrowserChrome* mWebBrowserChrome;
  nsIEmbeddingSiteWindow* mOwnerWin;
  nsIInterfaceRequestor* mOwnerRequestor;

  nsWeakPtr mWebBrowserChromeWeak; // nsIWebBrowserChrome

  // the objects that listen for chrome events like context menus and tooltips.
  // They are separate objects to avoid circular references between |this|
  // and the DOM.
  RefPtr<ChromeTooltipListener> mChromeTooltipListener;

  RefPtr<nsDocShellTreeOwner> mContentTreeOwner;

  nsCOMPtr<nsIPrompt> mPrompter;
  nsCOMPtr<nsIAuthPrompt> mAuthPrompter;
  nsCOMPtr<nsITabParent> mPrimaryTabParent;
};


// The class that listens to the chrome events and tells the embedding chrome to
// show tooltips, as appropriate. Handles registering itself with the DOM with
// AddChromeListeners() and removing itself with RemoveChromeListeners().
class ChromeTooltipListener final : public nsIDOMEventListener
{
protected:
  virtual ~ChromeTooltipListener();

public:
  NS_DECL_ISUPPORTS

  ChromeTooltipListener(nsWebBrowser* aInBrowser, nsIWebBrowserChrome* aInChrome);

  NS_DECL_NSIDOMEVENTLISTENER
  NS_IMETHOD MouseMove(mozilla::dom::Event* aMouseEvent);

  // Add/remove the relevant listeners, based on what interfaces the embedding
  // chrome implements.
  NS_IMETHOD AddChromeListeners();
  NS_IMETHOD RemoveChromeListeners();

private:
  // various delays for tooltips
  enum
  {
    kTooltipAutoHideTime = 5000,    // ms
    kTooltipMouseMoveTolerance = 7  // pixel tolerance for mousemove event
  };

  NS_IMETHOD AddTooltipListener();
  NS_IMETHOD RemoveTooltipListener();

  NS_IMETHOD ShowTooltip(int32_t aInXCoords, int32_t aInYCoords,
                         const nsAString& aInTipText,
                         const nsAString& aDirText);
  NS_IMETHOD HideTooltip();
  nsITooltipTextProvider* GetTooltipTextProvider();

  nsWebBrowser* mWebBrowser;
  nsCOMPtr<mozilla::dom::EventTarget> mEventTarget;
  nsCOMPtr<nsITooltipTextProvider> mTooltipTextProvider;

  // This must be a strong ref in order to make sure we can hide the tooltip if
  // the window goes away while we're displaying one. If we don't hold a strong
  // ref, the chrome might have been disposed of before we get a chance to tell
  // it, and no one would ever tell us of that fact.
  nsCOMPtr<nsIWebBrowserChrome> mWebBrowserChrome;

  bool mTooltipListenerInstalled;

  nsCOMPtr<nsITimer> mTooltipTimer;
  static void sTooltipCallback(nsITimer* aTimer, void* aListener);

  // Mouse coordinates for last mousemove event we saw
  int32_t mMouseClientX;
  int32_t mMouseClientY;

  // Mouse coordinates for tooltip event
  int32_t mMouseScreenX;
  int32_t mMouseScreenY;

  bool mShowingTooltip;
  bool mTooltipShownOnce;

  // The node hovered over that fired the timer. This may turn into the node
  // that triggered the tooltip, but only if the timer ever gets around to
  // firing. This is a strong reference, because the tooltip content can be
  // destroyed while we're waiting for the tooltip to pup up, and we need to
  // detect that. It's set only when the tooltip timer is created and launched.
  // The timer must either fire or be cancelled (or possibly released?), and we
  // release this reference in each of those cases. So we don't leak.
  nsCOMPtr<nsINode> mPossibleTooltipNode;
};

#endif /* nsDocShellTreeOwner_h__ */
