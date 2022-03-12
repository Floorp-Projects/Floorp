/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessible_h_
#define mozilla_a11y_xpcAccessible_h_

#include "nsIAccessible.h"

class nsIAccessible;

namespace mozilla {
namespace a11y {

class Accessible;
class LocalAccessible;

/**
 * XPCOM nsIAccessible interface implementation, used by xpcAccessibleGeneric
 * class.
 */
class xpcAccessible : public nsIAccessible {
 public:
  // nsIAccessible
  NS_IMETHOD GetParent(nsIAccessible** aParent) final;
  NS_IMETHOD GetNextSibling(nsIAccessible** aNextSibling) final;
  NS_IMETHOD GetPreviousSibling(nsIAccessible** aPreviousSibling) final;
  NS_IMETHOD GetFirstChild(nsIAccessible** aFirstChild) final;
  NS_IMETHOD GetLastChild(nsIAccessible** aLastChild) final;
  NS_IMETHOD GetChildCount(int32_t* aChildCount) final;
  NS_IMETHOD GetChildAt(int32_t aChildIndex, nsIAccessible** aChild) final;
  NS_IMETHOD GetChildren(nsIArray** aChildren) final;
  NS_IMETHOD GetIndexInParent(int32_t* aIndexInParent) final;

  NS_IMETHOD GetUniqueID(int64_t* aUniqueID) final;
  NS_IMETHOD GetDOMNode(nsINode** aDOMNode) final;
  NS_IMETHOD GetId(nsAString& aID) final;
  NS_IMETHOD GetDocument(nsIAccessibleDocument** aDocument) final;
  NS_IMETHOD GetRootDocument(nsIAccessibleDocument** aRootDocument) final;

  NS_IMETHOD GetRole(uint32_t* aRole) final;
  NS_IMETHOD GetState(uint32_t* aState, uint32_t* aExtraState) final;

  NS_IMETHOD GetDescription(nsAString& aDescription) final;
  NS_IMETHOD GetName(nsAString& aName) final;
  NS_IMETHOD GetLanguage(nsAString& aLanguage) final;
  NS_IMETHOD GetValue(nsAString& aValue) final;
  NS_IMETHOD GetHelp(nsAString& aHelp) final;

  NS_IMETHOD GetAccessKey(nsAString& aAccessKey) final;
  NS_IMETHOD GetKeyboardShortcut(nsAString& aKeyBinding) final;

  NS_IMETHOD GetAttributes(nsIPersistentProperties** aAttributes) final;

  NS_IMETHOD GetNativeInterface(nsISupports** aNativeInterface) final;

  NS_IMETHOD GetBounds(int32_t* aX, int32_t* aY, int32_t* aWidth,
                       int32_t* aHeight) final;
  NS_IMETHOD GetBoundsInCSSPixels(int32_t* aX, int32_t* aY, int32_t* aWidth,
                                  int32_t* aHeight) final;
  NS_IMETHOD GroupPosition(int32_t* aGroupLevel, int32_t* aSimilarItemsInGroup,
                           int32_t* aPositionInGroup) final;
  NS_IMETHOD GetRelationByType(uint32_t aType,
                               nsIAccessibleRelation** aRelation) final;
  NS_IMETHOD GetRelations(nsIArray** aRelations) final;

  NS_IMETHOD GetFocusedChild(nsIAccessible** aChild) final;
  NS_IMETHOD GetChildAtPoint(int32_t aX, int32_t aY,
                             nsIAccessible** aAccessible) final;
  NS_IMETHOD GetDeepestChildAtPoint(int32_t aX, int32_t aY,
                                    nsIAccessible** aAccessible) final;
  NS_IMETHOD GetDeepestChildAtPointInProcess(int32_t aX, int32_t aY,
                                             nsIAccessible** aAccessible) final;

  NS_IMETHOD SetSelected(bool aSelect) final;
  NS_IMETHOD TakeSelection() final;
  NS_IMETHOD TakeFocus() final;

  NS_IMETHOD GetActionCount(uint8_t* aActionCount) final;
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName) final;
  NS_IMETHOD GetActionDescription(uint8_t aIndex,
                                  nsAString& aDescription) final;
  NS_IMETHOD DoAction(uint8_t aIndex) final;

  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD ScrollTo(uint32_t aHow) final;
  NS_IMETHOD ScrollToPoint(uint32_t aCoordinateType, int32_t aX,
                           int32_t aY) final;

  NS_IMETHOD Announce(const nsAString& aAnnouncement, uint16_t aPriority) final;

 protected:
  xpcAccessible() {}
  virtual ~xpcAccessible() {}

 private:
  LocalAccessible* Intl();
  Accessible* IntlGeneric();

  xpcAccessible(const xpcAccessible&) = delete;
  xpcAccessible& operator=(const xpcAccessible&) = delete;
};

}  // namespace a11y
}  // namespace mozilla

#endif
