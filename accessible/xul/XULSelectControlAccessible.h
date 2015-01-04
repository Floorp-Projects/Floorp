/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULSelectControlAccessible_h__
#define mozilla_a11y_XULSelectControlAccessible_h__

#include "AccessibleWrap.h"
#include "nsIDOMXULSelectCntrlEl.h"

namespace mozilla {
namespace a11y {

/**
 * The basic implementation of accessible selection for XUL select controls.
 */
class XULSelectControlAccessible : public AccessibleWrap
{
public:
  XULSelectControlAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~XULSelectControlAccessible() {}

  // Accessible
  virtual void Shutdown() MOZ_OVERRIDE;

  // SelectAccessible
  virtual void SelectedItems(nsTArray<Accessible*>* aItems) MOZ_OVERRIDE;
  virtual uint32_t SelectedItemCount() MOZ_OVERRIDE;
  virtual Accessible* GetSelectedItem(uint32_t aIndex) MOZ_OVERRIDE;
  virtual bool IsItemSelected(uint32_t aIndex) MOZ_OVERRIDE;
  virtual bool AddItemToSelection(uint32_t aIndex) MOZ_OVERRIDE;
  virtual bool RemoveItemFromSelection(uint32_t aIndex) MOZ_OVERRIDE;
  virtual bool SelectAll() MOZ_OVERRIDE;
  virtual bool UnselectAll() MOZ_OVERRIDE;

  // Widgets
  virtual Accessible* CurrentItem() MOZ_OVERRIDE;
  virtual void SetCurrentItem(Accessible* aItem) MOZ_OVERRIDE;

protected:
  // nsIDOMXULMultiSelectControlElement inherits from this, so we'll always have
  // one of these if the widget is valid and not defunct
  nsCOMPtr<nsIDOMXULSelectControlElement> mSelectControl;
};

} // namespace a11y
} // namespace mozilla

#endif

