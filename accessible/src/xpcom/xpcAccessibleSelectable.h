/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleSelectable_h_
#define mozilla_a11y_xpcAccessibleSelectable_h_

#include "nsIAccessibleSelectable.h"

class nsIAccessible;
class nsIArray;

namespace mozilla {
namespace a11y {

class xpcAccessibleSelectable : public nsIAccessibleSelectable
{
public:
  NS_IMETHOD GetSelectedItems(nsIArray** aSelectedItems) MOZ_FINAL;
  NS_IMETHOD GetSelectedItemCount(uint32_t* aSelectedItemCount) MOZ_FINAL;
  NS_IMETHOD GetSelectedItemAt(uint32_t aIndex, nsIAccessible** aItem) MOZ_FINAL;
  NS_IMETHOD ScriptableIsItemSelected(uint32_t aIndex, bool* aIsSelected) MOZ_FINAL;
  NS_IMETHOD ScriptableAddItemToSelection(uint32_t aIndex) MOZ_FINAL;
  NS_IMETHOD ScriptableRemoveItemFromSelection(uint32_t aIndex) MOZ_FINAL;
  NS_IMETHOD ScriptableSelectAll(bool* aIsMultiSelect) MOZ_FINAL;
  NS_IMETHOD ScriptableUnselectAll() MOZ_FINAL;

private:
  xpcAccessibleSelectable() { }
  friend class Accessible;

  xpcAccessibleSelectable(const xpcAccessibleSelectable&) MOZ_DELETE;
  xpcAccessibleSelectable& operator =(const xpcAccessibleSelectable&) MOZ_DELETE;
};

} // namespace a11y
} // namespace mozilla

#endif
