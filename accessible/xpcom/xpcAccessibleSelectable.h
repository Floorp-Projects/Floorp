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

class Accessible;

/**
 * XPCOM nsIAccessibleSelectable inteface implementation, used by
 * xpcAccessibleGeneric class.
 */
class xpcAccessibleSelectable : public nsIAccessibleSelectable
{
public:
  // nsIAccessibleSelectable
  NS_IMETHOD GetSelectedItems(nsIArray** aSelectedItems) final override;
  NS_IMETHOD GetSelectedItemCount(uint32_t* aSelectedItemCount)
    final override;
  NS_IMETHOD GetSelectedItemAt(uint32_t aIndex, nsIAccessible** aItem)
    final override;
  NS_IMETHOD IsItemSelected(uint32_t aIndex, bool* aIsSelected)
    final override;
  NS_IMETHOD AddItemToSelection(uint32_t aIndex) final override;
  NS_IMETHOD RemoveItemFromSelection(uint32_t aIndex) final override;
  NS_IMETHOD SelectAll(bool* aIsMultiSelect) final override;
  NS_IMETHOD UnselectAll() final override;

protected:
  xpcAccessibleSelectable() { }
  virtual ~xpcAccessibleSelectable() {}

private:
  xpcAccessibleSelectable(const xpcAccessibleSelectable&) = delete;
  xpcAccessibleSelectable& operator =(const xpcAccessibleSelectable&) = delete;

  Accessible* Intl();
};

} // namespace a11y
} // namespace mozilla

#endif
