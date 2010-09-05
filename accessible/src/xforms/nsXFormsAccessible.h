/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsXFormsAccessible_H_
#define _nsXFormsAccessible_H_

#include "nsHyperTextAccessibleWrap.h"
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
class nsXFormsAccessible : public nsHyperTextAccessibleWrap,
                           public nsXFormsAccessibleBase
{
public:
  nsXFormsAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsIAccessible

  // Returns value of instance node that xforms element is bound to.
  NS_IMETHOD GetValue(nsAString& aValue);

  // Returns value of child xforms 'hint' element.
  NS_IMETHOD GetDescription(nsAString& aDescription);

  // nsAccessible

  // Returns value of child xforms 'label' element.
  virtual nsresult GetNameInternal(nsAString& aName);

  // Returns state of xforms element taking into account state of instance node
  // that it is bound to.
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);

  // Denies accessible nodes in anonymous content of xforms element by
  // always returning PR_FALSE value.
  virtual PRBool GetAllowsAnonChildAccessibles();

protected:
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
  void CacheSelectChildren(nsIDOMNode *aContainerNode = nsnull);
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
  nsXFormsContainerAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();

  // Allows accessible nodes in anonymous content of xforms element by
  // always returning PR_TRUE value.
  virtual PRBool GetAllowsAnonChildAccessibles();
};


/**
 * The class is base for accessible objects for XForms elements that have
 * editable area.
 */
class nsXFormsEditableAccessible : public nsXFormsAccessible
{
public:
  nsXFormsEditableAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsIAccessibleEditableText
  NS_IMETHOD GetAssociatedEditor(nsIEditor **aEditor);

  // nsAccessible
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};


/**
 * The class is base for accessible objects for XForms select and XForms
 * select1 elements.
 */
class nsXFormsSelectableAccessible : public nsXFormsEditableAccessible
{
public:
  nsXFormsSelectableAccessible(nsIContent *aContent, nsIWeakReference *aShell);

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

protected:
  nsIContent* GetItemByIndex(PRUint32* aIndex,
                             nsAccessible* aAccessible = nsnull);

  PRBool mIsSelect1Element;
};


/**
 * The class is base for accessible objects for XForms item elements.
 */
class nsXFormsSelectableItemAccessible : public nsXFormsAccessible
{
public:
  nsXFormsSelectableItemAccessible(nsIContent *aContent,
                                   nsIWeakReference *aShell);

  NS_IMETHOD GetValue(nsAString& aValue);
  NS_IMETHOD GetNumActions(PRUint8 *aCount);
  NS_IMETHOD DoAction(PRUint8 aIndex);

protected:
  PRBool IsItemSelected();
};

#endif

