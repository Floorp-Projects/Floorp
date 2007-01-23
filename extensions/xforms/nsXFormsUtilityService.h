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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Aaron Reed <aaronr@us.ibm.com> (original author)
 *  Alexander Surkov <surkov.alexander@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIXFormsUtilityService.h"

#include "nsIDOMNodeList.h"
#include "nsIContent.h"
#include "nsCOMArray.h"

class nsNodeList;

/**
 * The class implements "@mozilla.org/xforms-utility-service;1" XPCOM object
 * that is used outside XForms extension by core modules, for example by
 * accessibility.
 */

class nsXFormsUtilityService : public nsIXFormsUtilityService
{
public:
  NS_DECL_ISUPPORTS

  // nsIXFormsUtilityService
  NS_IMETHOD GetBuiltinTypeName(nsIDOMNode *aElement, nsAString& aName);
  NS_IMETHOD IsReadonly(nsIDOMNode *aElement, PRBool *aState);
  NS_IMETHOD IsRelevant(nsIDOMNode *aElement, PRBool *aState);
  NS_IMETHOD IsRequired(nsIDOMNode *aElement, PRBool *aState);
  NS_IMETHOD IsValid(nsIDOMNode *aElement, PRBool *aState);
  NS_IMETHOD IsInRange(nsIDOMNode *aElement, PRUint32 *aState);
  NS_IMETHOD GetValue(nsIDOMNode *aElement, nsAString& aValue);
  NS_IMETHOD Focus(nsIDOMNode *aElement);

  NS_IMETHOD GetRangeStart(nsIDOMNode *aElement, nsAString& aValue);
  NS_IMETHOD GetRangeEnd(nsIDOMNode *aElement, nsAString& aValue);
  NS_IMETHOD GetRangeStep(nsIDOMNode *aElement, nsAString& aValue);

  NS_IMETHOD GetEditor(nsIDOMNode *aElement, nsIEditor **aEditor);

  NS_IMETHOD IsDropmarkerOpen(nsIDOMNode *aElement, PRBool* aIsOpen);
  NS_IMETHOD ToggleDropmarkerState(nsIDOMNode *aElement);

  NS_IMETHOD GetSelectedItemForSelect1(nsIDOMNode *aElement,
                                       nsIDOMNode ** aItem);
  NS_IMETHOD SetSelectedItemForSelect1(nsIDOMNode *aElement, nsIDOMNode *aItem);

  NS_IMETHOD GetSelectedItemsForSelect(nsIDOMNode *aElement,
                                       nsIDOMNodeList **aItems);
  NS_IMETHOD AddItemToSelectionForSelect(nsIDOMNode *aElement,
                                         nsIDOMNode *aItem);
  NS_IMETHOD RemoveItemFromSelectionForSelect(nsIDOMNode *aElement,
                                              nsIDOMNode *aItem);
  NS_IMETHOD ClearSelectionForSelect(nsIDOMNode *aElement);
  NS_IMETHOD SelectAllItemsForSelect(nsIDOMNode *aElement);
  NS_IMETHOD IsSelectItemSelected(nsIDOMNode *aElement, nsIDOMNode *aItem,
                                  PRBool *aIsSelected);

  NS_IMETHOD GetSelectChildrenFor(nsIDOMNode *aElement,
                                  nsIDOMNodeList **aItemsList);

protected:
  // Append select child item/choices elements to given node list that are
  // hosted inside xforms:select, xforms:select1, xforms:choices or
  // xforms:itemset.
  nsresult GetSelectChildrenForNode(nsIDOMNode *aElement,
                                    nsNodeList *aNodeList);

  // Append select child item/choices elements to given node list that are
  // hosted inside xforms:select, xforms:select1, xforms:choices.
  nsresult GetSelectChildrenForNodeInternal(nsIDOMNode *aElement,
                                            nsNodeList *aNodeList);
};


/**
 * Implementation of nsIDOMNodeList interface.
 */
class nsNodeList : public nsIDOMNodeList
{
public:
  nsNodeList();
  ~nsNodeList();

  NS_DECL_ISUPPORTS

  // nsIDOMNodeList
  NS_DECL_NSIDOMNODELIST

  void AppendElement(nsIContent *aContent);

protected:
  nsCOMArray<nsIContent> mElements;
};

