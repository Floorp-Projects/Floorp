/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULTooltipListener_h__
#define nsXULTooltipListener_h__

#include "nsIDOMEventListener.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#ifdef MOZ_XUL
#include "nsITreeBoxObject.h"
#endif
#include "nsWeakPtr.h"
#include "mozilla/Attributes.h"

class nsIContent;
class nsITreeColumn;

namespace mozilla {
namespace dom {
class Event;
class MouseEvent;
} // namespace dom
} // namespace mozilla

class nsXULTooltipListener final : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  void MouseOut(mozilla::dom::Event* aEvent);
  void MouseMove(mozilla::dom::Event* aEvent);

  void AddTooltipSupport(nsIContent* aNode);
  void RemoveTooltipSupport(nsIContent* aNode);
  static nsXULTooltipListener* GetInstance() {
    if (!sInstance)
      sInstance = new nsXULTooltipListener();
    return sInstance;
  }

protected:

  nsXULTooltipListener();
  ~nsXULTooltipListener();

  // pref callback for when the "show tooltips" pref changes
  static bool sShowTooltips;

  void KillTooltipTimer();

#ifdef MOZ_XUL
  void CheckTreeBodyMove(mozilla::dom::MouseEvent* aMouseEvent);
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

  static nsXULTooltipListener* sInstance;
  static void ToolbarTipsPrefChanged(const char *aPref, void *aClosure);

  nsWeakPtr mSourceNode;
  nsWeakPtr mTargetNode;
  nsWeakPtr mCurrentTooltip;

  // a timer for showing the tooltip
  nsCOMPtr<nsITimer> mTooltipTimer;
  static void sTooltipCallback (nsITimer* aTimer, void* aListener);

  // screen coordinates of the last mousemove event, stored so that the
  // tooltip can be opened at this location.
  int32_t mMouseScreenX, mMouseScreenY;

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
  int32_t mLastTreeRow;
  nsCOMPtr<nsITreeColumn> mLastTreeCol;
#endif
};

#endif // nsXULTooltipListener
