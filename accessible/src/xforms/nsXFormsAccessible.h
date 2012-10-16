/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsXFormsAccessible_H_
#define _nsXFormsAccessible_H_

#include "HyperTextAccessibleWrap.h"
#include "nsIXFormsUtilityService.h"

#define NS_NAMESPACE_XFORMS "http://www.w3.org/2002/xforms"

/**
 * Utility class that provides access to nsIXFormsUtilityService.
 */
class nsXFormsAccessibleBase
{
public:
  nsXFormsAccessibleBase();

protected:
  // Used in GetActionName() methods.
  enum { eAction_Click = 0 };

  // Service allows to get some xforms functionality.
  static nsIXFormsUtilityService *sXFormsService;
};


/**
 * Every XForms element that is bindable to XForms model or is able to contain
 * XForms hint and XForms label elements should have accessible object. This
 * class is base class for accessible objects for these XForms elements.
 */
class nsXFormsAccessible : public HyperTextAccessibleWrap,
                           public nsXFormsAccessibleBase
{
public:
  nsXFormsAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  // Returns value of child xforms 'hint' element.
  virtual void Description(nsString& aDescription);

  // Returns value of instance node that xforms element is bound to.
  virtual void Value(nsString& aValue);

  // Returns state of xforms element taking into account state of instance node
  // that it is bound to.
  virtual uint64_t NativeState();
  virtual bool NativelyUnavailable() const;

  // Denies accessible nodes in anonymous content of xforms element by
  // always returning false value.
  virtual bool CanHaveAnonChildren();


protected:
  // Accessible
  // Returns value of child xforms 'label' element.
  virtual mozilla::a11y::ENameValueFlag NativeName(nsString& aName) MOZ_OVERRIDE;

  // Returns value of first child xforms element by tagname that is bound to
  // instance node.
  nsresult GetBoundChildElementValue(const nsAString& aTagName,
                                     nsAString& aValue);

  // Cache accessible child item/choices elements. For example, the method is
  // used for full appearance select/select1 elements or for their child choices
  // element. Note, those select/select1 elements that use native widget
  // for representation don't use the method since their item/choices elements
  // are hidden and therefore aren't accessible.
  //
  // @param aContainerNode - node that contains item elements
  void CacheSelectChildren(nsIDOMNode *aContainerNode = nullptr);
};


/**
 * This class is accessible object for XForms elements that provide accessible
 * object for itself as well for anonymous content. You should use this class
 * if accessible XForms element is complex, i.e. it is composed from elements
 * that should be accessible too. Especially for elements that have multiple
 * areas that a user can interact with or multiple visual areas. For example,
 * objects for XForms input[type="xsd:gMonth"] that contains combobox element
 * to choose month. It has an entryfield, a drop-down button and a drop-down
 * list, all of which need to be accessible. Another example would be
 * an XForms upload element since it is constructed from textfield and
 * 'pick up file' and 'clear file' buttons.
 */
class nsXFormsContainerAccessible : public nsXFormsAccessible
{
public:
  nsXFormsContainerAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual mozilla::a11y::role NativeRole();

  // Allows accessible nodes in anonymous content of xforms element by
  // always returning true value.
  virtual bool CanHaveAnonChildren();
};


/**
 * The class is base for accessible objects for XForms elements that have
 * editable area.
 */
class nsXFormsEditableAccessible : public nsXFormsAccessible
{
public:
  nsXFormsEditableAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // HyperTextAccessible
  virtual already_AddRefed<nsIEditor> GetEditor() const;

  // Accessible
  virtual uint64_t NativeState();
};


/**
 * The class is base for accessible objects for XForms select and XForms
 * select1 elements.
 */
class nsXFormsSelectableAccessible : public nsXFormsEditableAccessible
{
public:
  nsXFormsSelectableAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // SelectAccessible
  virtual bool IsSelect();
  virtual already_AddRefed<nsIArray> SelectedItems();
  virtual uint32_t SelectedItemCount();
  virtual Accessible* GetSelectedItem(uint32_t aIndex);
  virtual bool IsItemSelected(uint32_t aIndex);
  virtual bool AddItemToSelection(uint32_t aIndex);
  virtual bool RemoveItemFromSelection(uint32_t aIndex);
  virtual bool SelectAll();
  virtual bool UnselectAll();

protected:
  nsIContent* GetItemByIndex(uint32_t* aIndex,
                             Accessible* aAccessible = nullptr);

  bool mIsSelect1Element;
};


/**
 * The class is base for accessible objects for XForms item elements.
 */
class nsXFormsSelectableItemAccessible : public nsXFormsAccessible
{
public:
  nsXFormsSelectableItemAccessible(nsIContent* aContent,
                                   DocAccessible* aDoc);

  NS_IMETHOD DoAction(uint8_t aIndex);

  // Accessible
  virtual void Value(nsString& aValue);

  // ActionAccessible
  virtual uint8_t ActionCount();

protected:
  bool IsSelected();
};

#endif

