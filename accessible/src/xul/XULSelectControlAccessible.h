/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _XULSelectControlAccessible_H_
#define _XULSelectControlAccessible_H_

#include "nsAccessibleWrap.h"
#include "nsIDOMXULSelectCntrlEl.h"

/**
 * The basic implementation of accessible selection for XUL select controls.
 */
class XULSelectControlAccessible : public nsAccessibleWrap
{
public:
  XULSelectControlAccessible(nsIContent* aContent, nsDocAccessible* aDoc);
  virtual ~XULSelectControlAccessible() {}

  // nsAccessNode
  virtual void Shutdown();

  // SelectAccessible
  virtual bool IsSelect();
  virtual already_AddRefed<nsIArray> SelectedItems();
  virtual PRUint32 SelectedItemCount();
  virtual nsAccessible* GetSelectedItem(PRUint32 aIndex);
  virtual bool IsItemSelected(PRUint32 aIndex);
  virtual bool AddItemToSelection(PRUint32 aIndex);
  virtual bool RemoveItemFromSelection(PRUint32 aIndex);
  virtual bool SelectAll();
  virtual bool UnselectAll();

  // Widgets
  virtual nsAccessible* CurrentItem();
  virtual void SetCurrentItem(nsAccessible* aItem);

protected:
  // nsIDOMXULMultiSelectControlElement inherits from this, so we'll always have
  // one of these if the widget is valid and not defunct
  nsCOMPtr<nsIDOMXULSelectControlElement> mSelectControl;
};

#endif

