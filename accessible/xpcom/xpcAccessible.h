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

class xpcAccessible : public nsIAccessible
{
public:
  NS_IMETHOD GetParent(nsIAccessible** aParent) MOZ_FINAL;
  NS_IMETHOD GetNextSibling(nsIAccessible** aNextSibling) MOZ_FINAL;
  NS_IMETHOD GetPreviousSibling(nsIAccessible** aPreviousSibling) MOZ_FINAL;
  NS_IMETHOD GetFirstChild(nsIAccessible** aFirstChild) MOZ_FINAL;
  NS_IMETHOD GetLastChild(nsIAccessible** aLastChild) MOZ_FINAL;
  NS_IMETHOD GetChildCount(int32_t* aChildCount) MOZ_FINAL;
  NS_IMETHOD ScriptableGetChildAt(int32_t aChildIndex,
                                  nsIAccessible** aChild) MOZ_FINAL;
  NS_IMETHOD GetChildren(nsIArray** aChildren) MOZ_FINAL;
  NS_IMETHOD GetIndexInParent(int32_t* aIndexInParent) MOZ_FINAL;

  NS_IMETHOD GetDOMNode(nsIDOMNode** aDOMNode) MOZ_FINAL;
  NS_IMETHOD GetDocument(nsIAccessibleDocument** aDocument) MOZ_FINAL;
  NS_IMETHOD GetRootDocument(nsIAccessibleDocument** aRootDocument) MOZ_FINAL;

  NS_IMETHOD GetRole(uint32_t* aRole) MOZ_FINAL;
  NS_IMETHOD GetState(uint32_t* aState, uint32_t* aExtraState) MOZ_FINAL;

  NS_IMETHOD GetDescription(nsAString& aDescription) MOZ_FINAL;
  NS_IMETHOD GetName(nsAString& aName) MOZ_FINAL;
  NS_IMETHOD GetLanguage(nsAString& aLanguage) MOZ_FINAL;
  NS_IMETHOD GetValue(nsAString& aValue) MOZ_FINAL;
  NS_IMETHOD GetHelp(nsAString& aHelp) MOZ_FINAL;

  NS_IMETHOD GetAccessKey(nsAString& aAccessKey) MOZ_FINAL;
  NS_IMETHOD GetKeyboardShortcut(nsAString& aKeyBinding) MOZ_FINAL;

  NS_IMETHOD GetAttributes(nsIPersistentProperties** aAttributes) MOZ_FINAL;
  NS_IMETHOD GetBounds(int32_t* aX, int32_t* aY,
                       int32_t* aWidth, int32_t* aHeight) MOZ_FINAL;
  NS_IMETHOD ScriptableGroupPosition(int32_t* aGroupLevel,
                                     int32_t* aSimilarItemsInGroup,
                                     int32_t* aPositionInGroup) MOZ_FINAL;
  NS_IMETHOD GetRelationByType(uint32_t aType,
                               nsIAccessibleRelation** aRelation) MOZ_FINAL;
  NS_IMETHOD GetRelations(nsIArray** aRelations) MOZ_FINAL;

  NS_IMETHOD GetFocusedChild(nsIAccessible** aChild) MOZ_FINAL;
  NS_IMETHOD GetChildAtPoint(int32_t aX, int32_t aY,
                             nsIAccessible** aAccessible) MOZ_FINAL;
  NS_IMETHOD GetDeepestChildAtPoint(int32_t aX, int32_t aY,
                                    nsIAccessible** aAccessible) MOZ_FINAL;

  NS_IMETHOD ScriptableSetSelected(bool aSelect) MOZ_FINAL;
  NS_IMETHOD ExtendSelection() MOZ_FINAL;
  NS_IMETHOD ScriptableTakeSelection() MOZ_FINAL;
  NS_IMETHOD ScriptableTakeFocus() MOZ_FINAL;

  NS_IMETHOD GetActionCount(uint8_t* aActionCount) MOZ_FINAL;
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName) MOZ_FINAL;
  NS_IMETHOD GetActionDescription(uint8_t aIndex, nsAString& aDescription) MOZ_FINAL;
  NS_IMETHOD ScriptableDoAction(uint8_t aIndex) MOZ_FINAL;

  NS_IMETHOD ScriptableScrollTo(uint32_t aHow) MOZ_FINAL;
  NS_IMETHOD ScriptableScrollToPoint(uint32_t aCoordinateType,
                                     int32_t aX, int32_t aY) MOZ_FINAL;

private:
  xpcAccessible() { }
  friend class Accessible;

  xpcAccessible(const xpcAccessible&) MOZ_DELETE;
  xpcAccessible& operator =(const xpcAccessible&) MOZ_DELETE;
};

} // namespace a11y
} // namespace mozilla

#endif
