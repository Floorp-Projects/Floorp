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
  NS_IMETHOD GetSelectedItems(nsIArray** aSelectedItems) MOZ_FINAL;
  NS_IMETHOD GetSelectedItemCount(uint32_t* aSelectedItemCount) MOZ_FINAL;
  NS_IMETHOD GetSelectedItemAt(uint32_t aIndex, nsIAccessible** aItem) MOZ_FINAL;
  NS_IMETHOD IsItemSelected(uint32_t aIndex, bool* aIsSelected) MOZ_FINAL;
  NS_IMETHOD AddItemToSelection(uint32_t aIndex) MOZ_FINAL;
  NS_IMETHOD RemoveItemFromSelection(uint32_t aIndex) MOZ_FINAL;
  NS_IMETHOD SelectAll(bool* aIsMultiSelect) MOZ_FINAL;
  NS_IMETHOD UnselectAll() MOZ_FINAL;

protected:
  xpcAccessibleSelectable() { }
  virtual ~xpcAccessibleSelectable() {}

private:
  xpcAccessibleSelectable(const xpcAccessibleSelectable&) MOZ_DELETE;
  xpcAccessibleSelectable& operator =(const xpcAccessibleSelectable&) MOZ_DELETE;

  Accessible* Intl();
};

} // namespace a11y
} // namespace mozilla

#endif
