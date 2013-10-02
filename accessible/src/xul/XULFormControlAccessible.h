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
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);
  NS_IMETHOD DoAction(uint8_t index);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t NativeState();

  // ActionAccessible
  virtual uint8_t ActionCount();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
  virtual Accessible* ContainerWidget() const;

  virtual bool IsAcceptableChild(Accessible* aPossibleChild) const MOZ_OVERRIDE;

protected:
  // XULButtonAccessible
  bool ContainsMenu();
};


/**
 * Used for XUL checkbox element.
 */
class XULCheckboxAccessible : public LeafAccessible
{
public:
  enum { eAction_Click = 0 };
  XULCheckboxAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);
  NS_IMETHOD DoAction(uint8_t index);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t NativeState();

  // ActionAccessible
  virtual uint8_t ActionCount();
};

/**
 * Used for XUL dropmarker element.
 */
class XULDropmarkerAccessible : public LeafAccessible
{
public:
  enum { eAction_Click = 0 };
  XULDropmarkerAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);
  NS_IMETHOD DoAction(uint8_t index);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t NativeState();

  // ActionAccessible
  virtual uint8_t ActionCount();

private:
  bool DropmarkerOpen(bool aToggleOpen);
};

/**
 * Used for XUL groupbox element.
 */
class XULGroupboxAccessible : public AccessibleWrap
{
public:
  XULGroupboxAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual Relation RelationByType(uint32_t aRelationType);

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) MOZ_OVERRIDE;
};

/**
 * Used for XUL radio element (radio button).
 */
class XULRadioButtonAccessible : public RadioButtonAccessible
{

public:
  XULRadioButtonAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual uint64_t NativeState();
  virtual uint64_t NativeInteractiveState() const;

  // Widgets
  virtual Accessible* ContainerWidget() const;
};

/**
 * Used for XUL radiogroup element.
 */
class XULRadioGroupAccessible : public XULSelectControlAccessible
{
public:
  XULRadioGroupAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t NativeInteractiveState() const;

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
};

/**
 * Used for XUL statusbar element.
 */
class XULStatusBarAccessible : public AccessibleWrap
{
public:
  XULStatusBarAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
};

/**
 * Used for XUL toolbarbutton element.
 */
class XULToolbarButtonAccessible : public XULButtonAccessible
{
public:
  XULToolbarButtonAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual void GetPositionAndSizeInternal(int32_t *aPosInSet,
                                          int32_t *aSetSize);

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
  virtual mozilla::a11y::role NativeRole();

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) MOZ_OVERRIDE;
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
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t NativeState();
};

/**
 * Used for XUL textbox element.
 */
class XULTextFieldAccessible : public HyperTextAccessibleWrap
{
public:
  enum { eAction_Click = 0 };

  XULTextFieldAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);
  NS_IMETHOD DoAction(uint8_t index);

  // HyperTextAccessible
  virtual already_AddRefed<nsIEditor> GetEditor() const;

  // Accessible
  virtual void Value(nsString& aValue);
  virtual void ApplyARIAState(uint64_t* aState) const;
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t NativeState();
  virtual bool CanHaveAnonChildren();
  virtual bool IsAcceptableChild(Accessible* aPossibleChild) const MOZ_OVERRIDE;

  // ActionAccessible
  virtual uint8_t ActionCount();

protected:
  // Accessible
  virtual void CacheChildren();

  // HyperTextAccessible
  virtual already_AddRefed<nsFrameSelection> FrameSelection();

  // nsXULTextFieldAccessible
  already_AddRefed<nsIContent> GetInputField() const;
};

} // namespace a11y
} // namespace mozilla

#endif

