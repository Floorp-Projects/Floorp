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
#include "Units.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/Attributes.h"

class nsIContent;
class nsTreeColumn;

namespace mozilla {
namespace dom {
class Event;
class MouseEvent;
class XULTreeElement;
}  // namespace dom
class WidgetKeyboardEvent;
}  // namespace mozilla

class nsXULTooltipListener final : public nsIDOMEventListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  void MouseOut(mozilla::dom::Event* aEvent);
  void MouseMove(mozilla::dom::Event* aEvent);

  void AddTooltipSupport(nsIContent* aNode);
  void RemoveTooltipSupport(nsIContent* aNode);
  static nsXULTooltipListener* GetInstance() {
    if (!sInstance) sInstance = new nsXULTooltipListener();
    return sInstance;
  }

  static bool KeyEventHidesTooltip(const mozilla::WidgetKeyboardEvent&);
  static bool ShowTooltips();

 protected:
  nsXULTooltipListener();
  ~nsXULTooltipListener();

  void KillTooltipTimer();

  void CheckTreeBodyMove(mozilla::dom::MouseEvent* aMouseEvent);
  mozilla::dom::XULTreeElement* GetSourceTree();

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

  nsWeakPtr mSourceNode;
  nsWeakPtr mTargetNode;
  nsWeakPtr mCurrentTooltip;
  nsWeakPtr mPreviousMouseMoveTarget;

  // a timer for showing the tooltip
  nsCOMPtr<nsITimer> mTooltipTimer;
  static void sTooltipCallback(nsITimer* aTimer, void* aListener);

  // Screen coordinates of the last mousemove event, stored so that the tooltip
  // can be opened at this location.
  //
  // TODO(emilio): This duplicates a lot of code with ChromeTooltipListener.
  mozilla::LayoutDeviceIntPoint mMouseScreenPoint;

  // Tolerance for mousemove event
  static constexpr mozilla::LayoutDeviceIntCoord kTooltipMouseMoveTolerance = 7;

  // flag specifying if the tooltip has already been displayed by a MouseMove
  // event. The flag is reset on MouseOut so that the tooltip will display
  // the next time the mouse enters the node (bug #395668).
  bool mTooltipShownOnce;

  // special members for handling trees
  bool mIsSourceTree;
  bool mNeedTitletip;
  int32_t mLastTreeRow;
  RefPtr<nsTreeColumn> mLastTreeCol;
};

#endif  // nsXULTooltipListener
