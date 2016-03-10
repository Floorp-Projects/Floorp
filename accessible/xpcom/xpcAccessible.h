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
class AccessibleOrProxy;

/**
 * XPCOM nsIAccessible interface implementation, used by xpcAccessibleGeneric
 * class.
 */
class xpcAccessible : public nsIAccessible
{
public:
  // nsIAccessible
  NS_IMETHOD GetParent(nsIAccessible** aParent) final override;
  NS_IMETHOD GetNextSibling(nsIAccessible** aNextSibling) final override;
  NS_IMETHOD GetPreviousSibling(nsIAccessible** aPreviousSibling)
    final override;
  NS_IMETHOD GetFirstChild(nsIAccessible** aFirstChild) final override;
  NS_IMETHOD GetLastChild(nsIAccessible** aLastChild) final override;
  NS_IMETHOD GetChildCount(int32_t* aChildCount) final override;
  NS_IMETHOD GetChildAt(int32_t aChildIndex, nsIAccessible** aChild)
    final override;
  NS_IMETHOD GetChildren(nsIArray** aChildren) final override;
  NS_IMETHOD GetIndexInParent(int32_t* aIndexInParent) final override;

  NS_IMETHOD GetDOMNode(nsIDOMNode** aDOMNode) final override;
  NS_IMETHOD GetId(nsAString& aID) final override;
  NS_IMETHOD GetDocument(nsIAccessibleDocument** aDocument) final override;
  NS_IMETHOD GetRootDocument(nsIAccessibleDocument** aRootDocument)
    final override;

  NS_IMETHOD GetRole(uint32_t* aRole) final override;
  NS_IMETHOD GetState(uint32_t* aState, uint32_t* aExtraState)
    final override;

  NS_IMETHOD GetDescription(nsAString& aDescription) final override;
  NS_IMETHOD GetName(nsAString& aName) final override;
  NS_IMETHOD GetLanguage(nsAString& aLanguage) final override;
  NS_IMETHOD GetValue(nsAString& aValue) final override;
  NS_IMETHOD GetHelp(nsAString& aHelp) final override;

  NS_IMETHOD GetAccessKey(nsAString& aAccessKey) final override;
  NS_IMETHOD GetKeyboardShortcut(nsAString& aKeyBinding) final override;

  NS_IMETHOD GetAttributes(nsIPersistentProperties** aAttributes)
    final override;
  NS_IMETHOD GetBounds(int32_t* aX, int32_t* aY,
                       int32_t* aWidth, int32_t* aHeight) final override;
  NS_IMETHOD GroupPosition(int32_t* aGroupLevel, int32_t* aSimilarItemsInGroup,
                           int32_t* aPositionInGroup) final override;
  NS_IMETHOD GetRelationByType(uint32_t aType,
                               nsIAccessibleRelation** aRelation)
    final override;
  NS_IMETHOD GetRelations(nsIArray** aRelations) final override;

  NS_IMETHOD GetFocusedChild(nsIAccessible** aChild) final override;
  NS_IMETHOD GetChildAtPoint(int32_t aX, int32_t aY,
                             nsIAccessible** aAccessible) final override;
  NS_IMETHOD GetDeepestChildAtPoint(int32_t aX, int32_t aY,
                                    nsIAccessible** aAccessible)
    final override;

  NS_IMETHOD SetSelected(bool aSelect) final override;
  NS_IMETHOD TakeSelection() final override;
  NS_IMETHOD TakeFocus() final override;

  NS_IMETHOD GetActionCount(uint8_t* aActionCount) final override;
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName) final override;
  NS_IMETHOD GetActionDescription(uint8_t aIndex, nsAString& aDescription)
    final override;
  NS_IMETHOD DoAction(uint8_t aIndex) final override;

  NS_IMETHOD ScrollTo(uint32_t aHow) final override;
  NS_IMETHOD ScrollToPoint(uint32_t aCoordinateType,
                           int32_t aX, int32_t aY) final override;

protected:
  xpcAccessible() { }
  virtual ~xpcAccessible() {}

private:
  Accessible* Intl();
  AccessibleOrProxy IntlGeneric();

  xpcAccessible(const xpcAccessible&) = delete;
  xpcAccessible& operator =(const xpcAccessible&) = delete;
};

} // namespace a11y
} // namespace mozilla

#endif
