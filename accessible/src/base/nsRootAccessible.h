/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsRootAccessible_H_
#define _nsRootAccessible_H_

#include "nsAccessible.h"
#include "nsIAccessibleEventReceiver.h"
#include "nsIAccessibleEventListener.h"
#include "nsIAccessibleDocument.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMXULListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDocument.h"
#include "nsIAccessibilityService.h"
#include "nsIWebProgressListener.h"
#include "nsIWeakReference.h"
#include "nsITimer.h"
#include "nsIWebProgress.h"
#include "nsIScrollPositionListener.h"
#include "nsIScrollableView.h"
#include "nsHashtable.h"

const PRInt32 SCROLL_HASH_START_SIZE = 6;

class nsDocAccessibleMixin
{
  public:
    nsDocAccessibleMixin(nsIDocument *doc);
    nsDocAccessibleMixin(nsIWeakReference *aShell);
    virtual ~nsDocAccessibleMixin();
    
    NS_DECL_NSIACCESSIBLEDOCUMENT

  protected:
    NS_IMETHOD GetDocShellFromPS(nsIPresShell* aPresShell, nsIDocShell** aDocShell);
    nsCOMPtr<nsIDocument> mDocument;
};

class nsRootAccessible : public nsAccessible,
                         public nsDocAccessibleMixin,
                         public nsIAccessibleDocument,
                         public nsIAccessibleEventReceiver,
                         public nsIDOMFocusListener,
                         public nsIDOMFormListener,
                         public nsIDOMXULListener,
                         public nsIWebProgressListener,
                         public nsIScrollPositionListener,
                         public nsSupportsWeakReference

{
  NS_DECL_ISUPPORTS_INHERITED

  public:
    enum EBusyState {eBusyStateUninitialized, eBusyStateLoading, eBusyStateDone};

    nsRootAccessible(nsIWeakReference* aShell);
    virtual ~nsRootAccessible();

    /* attribute wstring accName; */
    NS_IMETHOD GetAccName(nsAString& aAccName);
    NS_IMETHOD GetAccValue(nsAString& aAccValue);
    NS_IMETHOD GetAccParent(nsIAccessible * *aAccParent);
    NS_IMETHOD GetAccRole(PRUint32 *aAccRole);
    NS_IMETHOD GetAccState(PRUint32 *aAccState);

    // ----- nsIAccessibleEventReceiver -------------------
    NS_IMETHOD AddAccessibleEventListener(nsIAccessibleEventListener *aListener);
    NS_IMETHOD RemoveAccessibleEventListener();

    // ----- nsIDOMEventListener --------------------------
    NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

    // ----- nsIDOMFocusListener --------------------------
    NS_IMETHOD Focus(nsIDOMEvent* aEvent);
    NS_IMETHOD Blur(nsIDOMEvent* aEvent);

    // ----- nsIDOMFormListener ---------------------------
    NS_IMETHOD Submit(nsIDOMEvent* aEvent);
    NS_IMETHOD Reset(nsIDOMEvent* aEvent);
    NS_IMETHOD Change(nsIDOMEvent* aEvent);
    NS_IMETHOD Select(nsIDOMEvent* aEvent);
    NS_IMETHOD Input(nsIDOMEvent* aEvent);

    // ----- nsIDOMXULListener ---------------------------
    NS_IMETHOD PopupShowing(nsIDOMEvent* aEvent);
    NS_IMETHOD PopupShown(nsIDOMEvent* aEvent);
    NS_IMETHOD PopupHiding(nsIDOMEvent* aEvent);
    NS_IMETHOD PopupHidden(nsIDOMEvent* aEvent);
    NS_IMETHOD Close(nsIDOMEvent* aEvent);
    NS_IMETHOD Command(nsIDOMEvent* aEvent);
    NS_IMETHOD Broadcast(nsIDOMEvent* aEvent);
    NS_IMETHOD CommandUpdate(nsIDOMEvent* aEvent);

    // ----- nsIScrollPositionListener ---------------------------
    NS_IMETHOD ScrollPositionWillChange(nsIScrollableView *aView, nscoord aX, nscoord aY);
    NS_IMETHOD ScrollPositionDidChange(nsIScrollableView *aView, nscoord aX, nscoord aY);

    NS_DECL_NSIACCESSIBLEDOCUMENT
    NS_DECL_NSIWEBPROGRESSLISTENER

  protected:
    NS_IMETHOD GetTargetNode(nsIDOMEvent *aEvent, nsCOMPtr<nsIDOMNode>& aTargetNode);
    virtual void GetBounds(nsRect& aRect, nsIFrame** aRelativeFrame);
    virtual nsIFrame* GetFrame();
    void FireAccessibleFocusEvent(nsIAccessible *focusAccessible, nsIDOMNode *focusNode);
    void AddScrollListener(nsIPresShell *aPresShell);
    void RemoveScrollListener(nsIPresShell *aPresShell);
    void AddContentDocListeners();
    void RemoveContentDocListeners();
    void FireDocLoadFinished();

    static void DocLoadCallback(nsITimer *aTimer, void *aClosure);
    static void ScrollTimerCallback(nsITimer *aTimer, void *aClosure);

    static PRUint32 gInstanceCount;

    // mListener is not a com pointer. We don't own the listener
    // it is the callers responsibility to remove the listener
    // otherwise we will get into circular referencing problems
    // We don't need a weak reference, because we're owned by this listener
    nsIAccessibleEventListener *mListener;

    static nsIDOMNode * gLastFocusedNode; // we do our own refcounting for this

    nsCOMPtr<nsITimer> mScrollWatchTimer;
    nsCOMPtr<nsITimer> mDocLoadTimer;
    nsCOMPtr<nsIWebProgress> mWebProgress;
    nsCOMPtr<nsIAccessibilityService> mAccService;
    EBusyState mBusy;
    PRPackedBool mIsNewDocument;

    // Used for tracking scroll events
    PRUint32 mScrollPositionChangedTicks;
    nsCOMPtr<nsIAccessibleCaret> mCaretAccessible;
};

#endif  
