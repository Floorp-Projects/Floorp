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
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
  virtual Accessible* ContainerWidget() const;

protected:

  // Accessible
  virtual void CacheChildren();

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
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();
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
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();

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
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual Relation RelationByType(PRUint32 aRelationType);
};

/**
 * Used for XUL radio element (radio button).
 */
class XULRadioButtonAccessible : public RadioButtonAccessible
{

public:
  XULRadioButtonAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual PRUint64 NativeState();
  virtual PRUint64 NativeInteractiveState() const;

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
  virtual PRUint64 NativeInteractiveState() const;

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
  virtual void GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                          PRInt32 *aSetSize);

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
  virtual nsresult GetNameInternal(nsAString& aName);
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
  virtual PRUint64 NativeState();
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
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 index);

  // HyperTextAccessible
  virtual already_AddRefed<nsIEditor> GetEditor() const;

  // Accessible
  virtual void Value(nsString& aValue);
  virtual void ApplyARIAState(PRUint64* aState) const;
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual bool CanHaveAnonChildren();

  // ActionAccessible
  virtual PRUint8 ActionCount();

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

