/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLSelectAccessible_h__
#define mozilla_a11y_HTMLSelectAccessible_h__

#include "HTMLFormControlAccessible.h"

namespace mozilla {
namespace a11y {

/**
  *  Selects, Listboxes and Comboboxes, are made up of a number of different
  *  widgets, some of which are shared between the two. This file contains
  *  all of the widgets for both of the Selects, for HTML only.
  *
  *  Listbox:
  *     - HTMLSelectListAccessible
  *        - HTMLSelectOptionAccessible
  *
  *  Comboboxes:
  *     - HTMLComboboxAccessible
  *        - HTMLComboboxListAccessible  [ inserted in accessible tree ]
  *           - HTMLSelectOptionAccessible(s)
  */

/*
 * The list that contains all the options in the select.
 */
class HTMLSelectListAccessible : public AccessibleWrap
{
public:

  HTMLSelectListAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~HTMLSelectListAccessible() {}

  // Accessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual bool IsAcceptableChild(nsIContent* aEl) const override;

  // SelectAccessible
  virtual bool SelectAll() override;
  virtual bool UnselectAll() override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual bool IsActiveWidget() const override;
  virtual bool AreItemsOperable() const override;
  virtual Accessible* CurrentItem() const override;
  virtual void SetCurrentItem(const Accessible* aItem) override;
};

/*
 * Options inside the select, contained within the list
 */
class HTMLSelectOptionAccessible : public HyperTextAccessibleWrap
{
public:
  enum { eAction_Select = 0 };

  HTMLSelectOptionAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~HTMLSelectOptionAccessible() {}

  // Accessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual uint64_t NativeInteractiveState() const override;

  virtual int32_t GetLevelInternal() override;
  virtual nsRect RelativeBounds(nsIFrame** aBoundingFrame) const override;
  virtual void SetSelected(bool aSelect) override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) const override;

  // Widgets
  virtual Accessible* ContainerWidget() const override;

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

private:

  /**
   * Return a select accessible the option belongs to if any.
   */
  Accessible* GetSelect() const
  {
    Accessible* parent = mParent;
    if (parent && parent->IsHTMLOptGroup())
      parent = parent->Parent();

    if (parent && parent->IsListControl()) {
      Accessible* combobox = parent->Parent();
      return combobox && combobox->IsCombobox() ? combobox : mParent;
    }

    return nullptr;
  }

  /**
   * Return a combobox accessible the option belongs to if any.
   */
  Accessible* GetCombobox() const
  {
    Accessible* parent = mParent;
    if (parent && parent->IsHTMLOptGroup())
      parent = parent->Parent();

    if (parent && parent->IsListControl()) {
      Accessible* combobox = parent->Parent();
      return combobox && combobox->IsCombobox() ? combobox : nullptr;
    }

    return nullptr;
  }
};

/*
 * Opt Groups inside the select, contained within the list
 */
class HTMLSelectOptGroupAccessible : public HTMLSelectOptionAccessible
{
public:

  HTMLSelectOptGroupAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    HTMLSelectOptionAccessible(aContent, aDoc)
    { mType = eHTMLOptGroupType; }
  virtual ~HTMLSelectOptGroupAccessible() {}

  // Accessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeInteractiveState() const override;
  virtual bool IsAcceptableChild(nsIContent* aEl) const override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) const override;
};

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

class HTMLComboboxListAccessible;

/*
 * A class the represents the HTML Combobox widget.
 */
class HTMLComboboxAccessible final : public AccessibleWrap
{
public:
  enum { eAction_Click = 0 };

  HTMLComboboxAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~HTMLComboboxAccessible() {}

  // Accessible
  virtual void Shutdown() override;
  virtual void Description(nsString& aDescription) override;
  virtual void Value(nsString& aValue) const override;
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual bool RemoveChild(Accessible* aChild) override;
  virtual bool IsAcceptableChild(nsIContent* aEl) const override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) const override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual bool IsActiveWidget() const override;
  virtual bool AreItemsOperable() const override;
  virtual Accessible* CurrentItem() const override;
  virtual void SetCurrentItem(const Accessible* aItem) override;

protected:
  /**
   * Return selected option.
   */
  Accessible* SelectedOption() const;

private:
  RefPtr<HTMLComboboxListAccessible> mListAccessible;
};

/*
 * A class that represents the window that lives to the right
 * of the drop down button inside the Select. This is the window
 * that is made visible when the button is pressed.
 */
class HTMLComboboxListAccessible : public HTMLSelectListAccessible
{
public:

  HTMLComboboxListAccessible(Accessible* aParent, nsIContent* aContent,
                             DocAccessible* aDoc);
  virtual ~HTMLComboboxListAccessible() {}

  // Accessible
  virtual nsIFrame* GetFrame() const override;
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual nsRect RelativeBounds(nsIFrame** aBoundingFrame) const override;
  virtual bool IsAcceptableChild(nsIContent* aEl) const override;

  // Widgets
  virtual bool IsActiveWidget() const override;
  virtual bool AreItemsOperable() const override;
};

} // namespace a11y
} // namespace mozilla

#endif
