/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#ifndef nsDocShellTreeOwner_h__
#define nsDocShellTreeOwner_h__

// Helper Classes
#include "nsCOMPtr.h"
#include "nsString.h"

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMDocument.h"
#include "nsIChromeEventHandler.h"
#include "nsIDOMEventReceiver.h"
#include "nsIWebBrowserSiteWindow.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsITimer.h"

#include "nsCommandHandler.h"

class nsWebBrowser;
class ChromeListener;

// {6D10C180-6888-11d4-952B-0020183BF181}
#define NS_ICDOCSHELLTREEOWNER_IID \
{ 0x6d10c180, 0x6888, 0x11d4, { 0x95, 0x2b, 0x0, 0x20, 0x18, 0x3b, 0xf1, 0x81 } }

/*
 * This is a fake 'hidden' interface that nsDocShellTreeOwner implements.
 * Classes such as nsCommandHandler can QI for this interface to be
 * sure that they're dealing with a valid nsDocShellTreeOwner and not some
 * other object that implements nsIDocShellTreeOwner.
 */
class nsICDocShellTreeOwner : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICDOCSHELLTREEOWNER_IID)
};


class nsDocShellTreeOwner : public nsIDocShellTreeOwner,
                            public nsIBaseWindow,
                            public nsIInterfaceRequestor,
                            public nsIWebProgressListener,
                            public nsICDocShellTreeOwner,
                            public nsSupportsWeakReference
{
friend class nsWebBrowser;
friend class nsCommandHandler;

public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIBASEWINDOW
    NS_DECL_NSIDOCSHELLTREEOWNER
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

    nsresult   FindChildWithName(const PRUnichar *aName, 
                 PRBool aRecurse, nsIDocShellTreeItem* aRequestor, 
                 nsIDocShellTreeItem **aFoundItem);
    nsresult   FindItemWithNameAcrossWindows(const PRUnichar* aName,
                 nsIDocShellTreeItem **aFoundItem);

    void AddToWatcher();
    void RemoveFromWatcher();

protected:

   // Weak References
   nsWebBrowser*           mWebBrowser;
   nsIDocShellTreeOwner*   mTreeOwner;
   nsIDocShellTreeItem*    mPrimaryContentShell; 

   nsIWebBrowserChrome*    mWebBrowserChrome;
   nsIWebBrowserSiteWindow* mOwnerWin;
   nsIInterfaceRequestor*  mOwnerRequestor;
   
    // the object that listens for chrome events like context menus and tooltips. 
    // It is a separate object to avoid circular references between |this|
    // and the DOM. This is a strong, owning ref.
   ChromeListener*         mChromeListener;
};


//
// class ChromeListener
//
// The class that listens to the chrome events and tells the embedding
// chrome to show things like context menus or tooltips, as appropriate.
// Handles registering itself with the DOM with AddChromeListeners()
// and removing itself with RemoveChromeListeners().
//
// NOTE: Because of a leak/bug in the EventListenerManager having to do with
//       a single listener with multiple IID's, we need to addref/release
//       this class instead of just owning it and calling delete when we
//       know we're done. This also forces us to make RemoveChromeListeners()
//       public when it doesn't need to be. This is a reminder to fix when I
//       fix the ELM bug. (pinkerton)
//
class ChromeListener : public nsIDOMMouseListener,
                       public nsIDOMKeyListener,
                       public nsIDOMMouseMotionListener
{
public:
  NS_DECL_ISUPPORTS
  
  ChromeListener ( nsWebBrowser* inBrowser, nsIWebBrowserChrome* inChrome ) ;
  ~ChromeListener ( ) ;

	  // nsIDOMMouseListener
	virtual nsresult HandleEvent(nsIDOMEvent* aEvent) {	return NS_OK; }
	virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent);
	virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent);

    // nsIDOMMouseMotionListener
  virtual nsresult MouseMove(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragMove(nsIDOMEvent* aMouseEvent) { return NS_OK; };

    // nsIDOMKeyListener
  virtual nsresult KeyDown(nsIDOMEvent* aKeyEvent) ;
  virtual nsresult KeyUp(nsIDOMEvent* aKeyEvent) ;
  virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent) ;

    // Add/remove the relevant listeners, based on what interfaces
    // the embedding chrome implements.
  NS_IMETHOD AddChromeListeners();
  NS_IMETHOD RemoveChromeListeners();

private:

  NS_IMETHOD AddContextMenuListener();
  NS_IMETHOD AddTooltipListener();
  NS_IMETHOD RemoveContextMenuListener();
  NS_IMETHOD RemoveTooltipListener();

  nsWebBrowser* mWebBrowser;
  nsCOMPtr<nsIDOMEventReceiver> mEventReceiver;
  
    // This must be a strong ref in order to make sure we can hide the tooltip
    // if the window goes away while we're displaying one. If we don't hold
    // a strong ref, the chrome might have been disposed of before we get a chance
    // to tell it, and no one would ever tell us of that fact.
  nsCOMPtr<nsIWebBrowserChrome> mWebBrowserChrome;

  PRPackedBool mContextMenuListenerInstalled;
  PRPackedBool mTooltipListenerInstalled;

private:

    // various delays for tooltips
  enum {
    kTooltipAutoHideTime = 5000,       // 5000ms = 5 seconds
    kTooltipShowTime = 500             // 500ms = 0.5 seconds
  };

  NS_IMETHOD ShowTooltip ( PRInt32 inXCoords, PRInt32 inYCoords, const nsAReadableString & inTipText ) ;
  NS_IMETHOD HideTooltip ( ) ;

    // Determine if there is a TITLE attribute. Returns |PR_TRUE| if there is,
    // and sets the text in |outText|.
  PRBool FindTitleText ( nsIDOMNode* inNode, nsAWritableString & outText ) ;

  nsCOMPtr<nsITimer> mTooltipTimer;
  static void sTooltipCallback ( nsITimer* aTimer, void* aListener ) ;
  PRInt32 mMouseClientX, mMouseClientY;       // mouse coordinates for tooltip event
  PRBool mShowingTooltip;

    // a timer for auto-hiding the tooltip after a certain delay
  nsCOMPtr<nsITimer> mAutoHideTimer;
  static void sAutoHideCallback ( nsITimer* aTimer, void* aListener ) ;
  void CreateAutoHideTimer ( ) ;

    // The node hovered over that fired the timer. This may turn into the node that
    // triggered the tooltip, but only if the timer ever gets around to firing.
    // This is a strong reference, because the tooltip content can be destroyed while we're
    // waiting for the tooltip to pup up, and we need to detect that.
    // It's set only when the tooltip timer is created and launched. The timer must
    // either fire or be cancelled (or possibly released?), and we release this
    // reference in each of those cases. So we don't leak.
  nsCOMPtr<nsIDOMNode> mPossibleTooltipNode;

}; // ChromeListener




#endif /* nsDocShellTreeOwner_h__ */












