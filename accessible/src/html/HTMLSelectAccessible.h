/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLSelectAccessible_h__
#define mozilla_a11y_HTMLSelectAccessible_h__

#include "HTMLFormControlAccessible.h"

class nsIMutableArray;

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
  virtual a11y::role NativeRole();
  virtual uint64_t NativeState();

  // SelectAccessible
  virtual bool SelectAll();
  virtual bool UnselectAll();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
  virtual Accessible* CurrentItem();
  virtual void SetCurrentItem(Accessible* aItem);

protected:

  // Accessible
  virtual void CacheChildren();
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

  // nsIAccessible
  NS_IMETHOD DoAction(uint8_t index);
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);
  NS_IMETHOD SetSelected(bool aSelect);

  // Accessible
  virtual a11y::role NativeRole();
  virtual uint64_t NativeState();
  virtual uint64_t NativeInteractiveState() const;

  virtual int32_t GetLevelInternal();
  virtual void GetBoundsRect(nsRect& aTotalBounds, nsIFrame** aBoundingFrame);

  // ActionAccessible
  virtual uint8_t ActionCount();

  // Widgets
  virtual Accessible* ContainerWidget() const;

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) MOZ_OVERRIDE;

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
      return combobox && combobox->IsCombobox() ? combobox : mParent.get();
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

  // nsIAccessible
  NS_IMETHOD DoAction(uint8_t index);
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);

  // Accessible
  virtual a11y::role NativeRole();
  virtual uint64_t NativeInteractiveState() const;

  // ActionAccessible
  virtual uint8_t ActionCount();
};

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

class HTMLComboboxListAccessible;

/*
 * A class the represents the HTML Combobox widget.
 */
class HTMLComboboxAccessible : public AccessibleWrap
{
public:
  enum { eAction_Click = 0 };

  HTMLComboboxAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~HTMLComboboxAccessible() {}

  // nsIAccessible
  NS_IMETHOD DoAction(uint8_t index);
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);

  // Accessible
  virtual void Shutdown();
  virtual void Description(nsString& aDescription);
  virtual void Value(nsString& aValue);
  virtual a11y::role NativeRole();
  virtual uint64_t NativeState();
  virtual void InvalidateChildren();

  // ActionAccessible
  virtual uint8_t ActionCount();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
  virtual Accessible* CurrentItem();
  virtual void SetCurrentItem(Accessible* aItem);

protected:
  // Accessible
  virtual void CacheChildren();

  /**
   * Return selected option.
   */
  Accessible* SelectedOption() const;

private:
  nsRefPtr<HTMLComboboxListAccessible> mListAccessible;
};

/*
 * A class that represents the window that lives to the right
 * of the drop down button inside the Select. This is the window
 * that is made visible when the button is pressed.
 */
class HTMLComboboxListAccessible : public HTMLSelectListAccessible
{
public:

  HTMLComboboxListAccessible(nsIAccessible* aParent, nsIContent* aContent,
                             DocAccessible* aDoc);
  virtual ~HTMLComboboxListAccessible() {}

  // Accessible
  virtual nsIFrame* GetFrame() const;
  virtual a11y::role NativeRole();
  virtual uint64_t NativeState();
  virtual void GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame);

  // Widgets
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
};

} // namespace a11y
} // namespace mozilla

#endif
