/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULTooltipListener_h__
#define nsXULTooltipListener_h__

#include "nsIDOMEventListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#ifdef MOZ_XUL
#include "nsITreeBoxObject.h"
#include "nsITreeColumns.h"
#endif
#include "nsWeakPtr.h"

class nsXULTooltipListener : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  void MouseOut(nsIDOMEvent* aEvent);
  void MouseMove(nsIDOMEvent* aEvent);

  nsresult AddTooltipSupport(nsIContent* aNode);
  nsresult RemoveTooltipSupport(nsIContent* aNode);
  static nsXULTooltipListener* GetInstance() {
    if (!mInstance)
      mInstance = new nsXULTooltipListener();
    return mInstance;
  }
  static void ClearTooltipCache() { mInstance = nsnull; }

protected:

  nsXULTooltipListener();
  ~nsXULTooltipListener();

  // pref callback for when the "show tooltips" pref changes
  static bool sShowTooltips;
  static PRUint32 sTooltipListenerCount;

  void KillTooltipTimer();

#ifdef MOZ_XUL
  void CheckTreeBodyMove(nsIDOMMouseEvent* aMouseEvent);
  nsresult GetSourceTreeBoxObject(nsITreeBoxObject** aBoxObject);
#endif

  nsresult ShowTooltip();
  void LaunchTooltip();
  nsresult HideTooltip();
  nsresult DestroyTooltip();
  // This method tries to find a tooltip for aTarget.
  nsresult FindTooltip(nsIContent* aTarget, nsIContent** aTooltip);
  // This method calls FindTooltip and checks that the tooltip
  // can be really used (i.e. tooltip is not a menu).
  nsresult GetTooltipFor(nsIContent* aTarget, nsIContent** aTooltip);

  static nsXULTooltipListener* mInstance;
  static int ToolbarTipsPrefChanged(const char *aPref, void *aClosure);

  nsWeakPtr mSourceNode;
  nsWeakPtr mTargetNode;
  nsWeakPtr mCurrentTooltip;

  // a timer for showing the tooltip
  nsCOMPtr<nsITimer> mTooltipTimer;
  static void sTooltipCallback (nsITimer* aTimer, void* aListener);

  // screen coordinates of the last mousemove event, stored so that the
  // tooltip can be opened at this location.
  PRInt32 mMouseScreenX, mMouseScreenY;

  // various constants for tooltips
  enum {
    kTooltipMouseMoveTolerance = 7     // 7 pixel tolerance for mousemove event
  };

  // flag specifying if the tooltip has already been displayed by a MouseMove
  // event. The flag is reset on MouseOut so that the tooltip will display
  // the next time the mouse enters the node (bug #395668).
  bool mTooltipShownOnce;

#ifdef MOZ_XUL
  // special members for handling trees
  bool mIsSourceTree;
  bool mNeedTitletip;
  PRInt32 mLastTreeRow;
  nsCOMPtr<nsITreeColumn> mLastTreeCol;
#endif
};

#endif // nsXULTooltipListener
