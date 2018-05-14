/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_XULFormControlAccessible_H_
#define MOZILLA_A11Y_XULFormControlAccessible_H_

// NOTE: alphabetically ordered
#include "AccessibleWrap.h"
#include "FormControlAccessible.h"
#include "HyperTextAccessibleWrap.h"
#include "XULSelectControlAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * Used for XUL progressmeter element.
 */
typedef ProgressMeterAccessible<100> XULProgressMeterAccessible;

/**
 * Used for XUL button.
 *
 * @note  Don't inherit from LeafAccessible - it doesn't allow children
 *         and a button can have a dropmarker child.
 */
class XULButtonAccessible : public AccessibleWrap
{
public:
  enum { eAction_Click = 0 };
  XULButtonAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(XULButtonAccessible, AccessibleWrap)

  // Accessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() override;

  // ActionAccessible
  virtual uint8_t ActionCount() override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual bool IsActiveWidget() const override;
  virtual bool AreItemsOperable() const override;
  virtual Accessible* ContainerWidget() const override;

  virtual bool IsAcceptableChild(nsIContent* aEl) const override;

protected:
  virtual ~XULButtonAccessible();

  // XULButtonAccessible
  bool ContainsMenu() const;
};


/**
 * Used for XUL checkbox element.
 */
class XULCheckboxAccessible : public LeafAccessible
{
public:
  enum { eAction_Click = 0 };
  XULCheckboxAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() override;

  // ActionAccessible
  virtual uint8_t ActionCount() override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) override;
};

/**
 * Used for XUL dropmarker element.
 */
class XULDropmarkerAccessible : public LeafAccessible
{
public:
  enum { eAction_Click = 0 };
  XULDropmarkerAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() override;

  // ActionAccessible
  virtual uint8_t ActionCount() override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) override;

private:
  bool DropmarkerOpen(bool aToggleOpen) const;
};

/**
 * Used for XUL groupbox element.
 */
class XULGroupboxAccessible final : public AccessibleWrap
{
public:
  XULGroupboxAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual Relation RelationByType(RelationType aType) override;

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) override;
};

/**
 * Used for XUL radio element (radio button).
 */
class XULRadioButtonAccessible : public RadioButtonAccessible
{

public:
  XULRadioButtonAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual uint64_t NativeState() override;
  virtual uint64_t NativeInteractiveState() const override;

  // Widgets
  virtual Accessible* ContainerWidget() const override;
};

/**
 * Used for XUL radiogroup element.
 */
class XULRadioGroupAccessible : public XULSelectControlAccessible
{
public:
  XULRadioGroupAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeInteractiveState() const override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual bool IsActiveWidget() const override;
  virtual bool AreItemsOperable() const override;
};

/**
 * Used for XUL statusbar element.
 */
class XULStatusBarAccessible : public AccessibleWrap
{
public:
  XULStatusBarAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() const override;
};

/**
 * Used for XUL toolbarbutton element.
 */
class XULToolbarButtonAccessible : public XULButtonAccessible
{
public:
  XULToolbarButtonAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual void GetPositionAndSizeInternal(int32_t* aPosInSet,
                                          int32_t* aSetSize) override;

  // nsXULToolbarButtonAccessible
  static bool IsSeparator(Accessible* aAccessible);
};

/**
 * Used for XUL toolbar element.
 */
class XULToolbarAccessible : public AccessibleWrap
{
public:
  XULToolbarAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() const override;

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) override;
};

/**
 * Used for XUL toolbarseparator element.
 */
class XULToolbarSeparatorAccessible : public LeafAccessible
{
public:
  XULToolbarSeparatorAccessible(nsIContent* aContent,
                                DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() override;
};

} // namespace a11y
} // namespace mozilla

#endif

