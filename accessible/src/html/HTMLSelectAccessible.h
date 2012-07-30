/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLSelectAccessible_h__
#define mozilla_a11y_HTMLSelectAccessible_h__

#include "HTMLFormControlAccessible.h"
#include "nsIDOMHTMLOptionsCollection.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMNode.h"

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
  virtual PRUint64 NativeState();

  // SelectAccessible
  virtual bool IsSelect();
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

  // HTMLSelectListAccessible

  /**
   * Recursive helper for CacheChildren().
   */
  void CacheOptSiblings(nsIContent* aParentContent);
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
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD SetSelected(bool aSelect);

  // Accessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual PRUint64 NativeInteractiveState() const;

  virtual PRInt32 GetLevelInternal();
  virtual void GetBoundsRect(nsRect& aTotalBounds, nsIFrame** aBoundingFrame);

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // Widgets
  virtual Accessible* ContainerWidget() const;

private:

  /**
   * Return a select accessible the option belongs to if any.
   */
  Accessible* GetSelect() const
  {
    if (mParent && mParent->IsListControl()) {
      Accessible* combobox = mParent->Parent();
      return combobox && combobox->IsCombobox() ? combobox : mParent.get();
    }

    return nullptr;
  }

  /**
   * Return a combobox accessible the option belongs to if any.
   */
  Accessible* GetCombobox() const
  {
    if (mParent && mParent->IsListControl()) {
      Accessible* combobox = mParent->Parent();
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

  HTMLSelectOptGroupAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~HTMLSelectOptGroupAccessible() {}

  // nsIAccessible
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // Accessible
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeInteractiveState() const;

  // ActionAccessible
  virtual PRUint8 ActionCount();

protected:
  // Accessible
  virtual void CacheChildren();
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
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  // nsAccessNode
  virtual void Shutdown();

  // Accessible
  virtual void Description(nsString& aDescription);
  virtual void Value(nsString& aValue);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual void InvalidateChildren();

  // ActionAccessible
  virtual PRUint8 ActionCount();

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

  // nsAccessNode
  virtual nsIFrame* GetFrame() const;
  virtual bool IsPrimaryForNode() const;

  // Accessible
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual void GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame);

  // Widgets
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
};

} // namespace a11y
} // namespace mozilla

#endif
