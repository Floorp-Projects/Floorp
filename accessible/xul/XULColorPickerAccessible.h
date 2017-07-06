/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULColorPickerAccessible_h__
#define mozilla_a11y_XULColorPickerAccessible_h__

#include "AccessibleWrap.h"

namespace mozilla {
namespace a11y {

/**
 * Used for color button in colorpicker palette.
 */
class XULColorPickerTileAccessible : public AccessibleWrap
{
public:
  XULColorPickerTileAccessible(nsIContent* aContent,
                               DocAccessible* aDoc);

  // Accessible
  virtual void Value(nsString& aValue) override;
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;
  virtual uint64_t NativeInteractiveState() const override;

  // Widgets
  virtual Accessible* ContainerWidget() const override;
};


/**
 * Used for colorpicker button (xul:colorpicker@type="button").
 */
class XULColorPickerAccessible : public XULColorPickerTileAccessible
{
public:
  XULColorPickerAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual bool IsActiveWidget() const override;
  virtual bool AreItemsOperable() const override;

  virtual bool IsAcceptableChild(nsIContent* aEl) const override;
};

} // namespace a11y
} // namespace mozilla

#endif
