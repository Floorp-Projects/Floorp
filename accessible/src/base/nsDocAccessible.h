/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsDocAccessible_H_
#define _nsDocAccessible_H_

#include "nsAccessibleWrap.h"
#include "nsIAccessibleDocument.h"
#include "nsIDocument.h"
#include "nsIAccessibleEventReceiver.h"
#include "nsHashtable.h"
#include "nsIWebProgressListener.h"
#include "nsITimer.h"
#include "nsIWebProgress.h"
#include "nsIScrollPositionListener.h"
#include "nsIWeakReference.h"

class nsIScrollableView;

const PRUint32 kDefaultCacheSize = 256;

class nsDocAccessible : public nsAccessibleWrap,
                        public nsIAccessibleDocument,
                        public nsIAccessibleEventReceiver,
                        public nsIWebProgressListener,
                        public nsIScrollPositionListener,
                        public nsSupportsWeakReference
{
  enum EBusyState {eBusyStateUninitialized, eBusyStateLoading, eBusyStateDone};

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLEDOCUMENT
  NS_DECL_NSIACCESSIBLEEVENTRECEIVER

  public:
    nsDocAccessible(nsIDOMNode *aNode, nsIWeakReference* aShell);
    virtual ~nsDocAccessible();

    NS_IMETHOD GetAccRole(PRUint32 *aAccRole);
    NS_IMETHOD GetAccName(nsAString& aAccName);
    NS_IMETHOD GetAccValue(nsAString& aAccValue);
    NS_IMETHOD GetAccState(PRUint32 *aAccState);

    // ----- nsIScrollPositionListener ---------------------------
    NS_IMETHOD ScrollPositionWillChange(nsIScrollableView *aView, nscoord aX, nscoord aY);
    NS_IMETHOD ScrollPositionDidChange(nsIScrollableView *aView, nscoord aX, nscoord aY);

    NS_DECL_NSIWEBPROGRESSLISTENER

    // nsIAccessNode
    NS_IMETHOD Shutdown();

  protected:  
    virtual void GetBounds(nsRect& aRect, nsIFrame** aRelativeFrame);
    virtual nsIFrame* GetFrame();
    void AddContentDocListeners();
    void RemoveContentDocListeners();
    void AddScrollListener(nsIPresShell *aPresShell);
    void RemoveScrollListener(nsIPresShell *aPresShell);
    void FireDocLoadFinished();
    static void DocLoadCallback(nsITimer *aTimer, void *aClosure);
    static void ScrollTimerCallback(nsITimer *aTimer, void *aClosure);

#ifdef OLD_HASH
    nsSupportsHashtable *mAccessNodeCache;
#else
    nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode> *mAccessNodeCache;
#endif
    void *mWnd;
    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsITimer> mScrollWatchTimer;
    nsCOMPtr<nsITimer> mDocLoadTimer;
    nsCOMPtr<nsIWebProgress> mWebProgress;
    EBusyState mBusy;
    PRUint16 mScrollPositionChangedTicks; // Used for tracking scroll events
    PRPackedBool mIsNewDocument;
};


#endif  
