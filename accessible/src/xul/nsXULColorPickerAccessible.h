/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXULColorPickerAccessible_H_
#define _nsXULColorPickerAccessible_H_

#include "AccessibleWrap.h"

/**
 * Used for color button in colorpicker palette.
 */
class nsXULColorPickerTileAccessible : public AccessibleWrap
{
public:
  nsXULColorPickerTileAccessible(nsIContent* aContent,
                                 DocAccessible* aDoc);

  // Accessible
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // Widgets
  virtual Accessible* ContainerWidget() const;
};


/**
 * Used for colorpicker button (xul:colorpicker@type="button").
 */
class nsXULColorPickerAccessible : public nsXULColorPickerTileAccessible
{
public:
  nsXULColorPickerAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;

protected:

  // Accessible
  virtual void CacheChildren();
};

#endif  
