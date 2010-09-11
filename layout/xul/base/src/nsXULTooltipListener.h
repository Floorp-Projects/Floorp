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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsXULTooltipListener_h__
#define nsXULTooltipListener_h__

#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMKeyListener.h"
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

class nsXULTooltipListener : public nsIDOMMouseListener,
                             public nsIDOMMouseMotionListener,
                             public nsIDOMKeyListener
{
public:
  NS_DECL_ISUPPORTS

  // nsIDOMMouseListener
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);

  // nsIDOMMouseMotionListener
  NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  NS_IMETHOD MouseMove(nsIDOMEvent* aMouseEvent);

  // nsIDOMKeyListener
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent) { return NS_OK; }
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent) { return NS_OK; }

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

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
  static PRBool sShowTooltips;
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
    kTooltipMouseMoveTolerance = 7,    // 7 pixel tolerance for mousemove event
    kTooltipShowTime = 500             // 500ms = 0.5 seconds
  };

  // flag specifying if the tooltip has already been displayed by a MouseMove
  // event. The flag is reset on MouseOut so that the tooltip will display
  // the next time the mouse enters the node (bug #395668).
  PRBool mTooltipShownOnce;

#ifdef MOZ_XUL
  // special members for handling trees
  PRBool mIsSourceTree;
  PRBool mNeedTitletip;
  PRInt32 mLastTreeRow;
  nsCOMPtr<nsITreeColumn> mLastTreeCol;
#endif
};

#endif // nsXULTooltipListener
