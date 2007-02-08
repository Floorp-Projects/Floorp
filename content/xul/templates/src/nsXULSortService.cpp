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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Putterman          <putterman@netscape.com>
 *   Pierre Phaneuf           <pp@ludusdesign.com>
 *   Chase Tingley            <tingley@sundell.net>
 *   Neil Deakin              <enndeakin@sympatico.ca>
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

/*
  This file provides the implementation for the sort service manager.
 */

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"
#include "nsXULContentUtils.h"
#include "nsString.h"
#include "nsQuickSort.h"
#include "nsXULSortService.h"
#include "nsIDOMXULElement.h"
#include "nsIXULTemplateBuilder.h"
#include "nsTemplateMatch.h"

NS_IMPL_ISUPPORTS1(XULSortServiceImpl, nsIXULSortService)

void
XULSortServiceImpl::SetSortHints(nsIContent *aNode, nsSortState* aSortState)
{
  // set sort and sortDirection attributes when is sort is done
  aNode->SetAttr(kNameSpaceID_None, nsGkAtoms::sort,
                 aSortState->sort, PR_TRUE);

  nsAutoString direction;
  if (aSortState->direction == nsSortState_descending)
    direction.AssignLiteral("descending");
  else if (aSortState->direction == nsSortState_ascending)
    direction.AssignLiteral("ascending");
  aNode->SetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection,
                 direction, PR_TRUE);

  // for trees, also set the sort info on the currently sorted column
  if (aNode->NodeInfo()->Equals(nsGkAtoms::tree, kNameSpaceID_XUL)) {
    if (aSortState->sortKeys.Count() >= 1) {
      nsAutoString sortkey;
      aSortState->sortKeys[0]->ToString(sortkey);
      SetSortColumnHints(aNode, sortkey, direction);
    }
  }
}

void
XULSortServiceImpl::SetSortColumnHints(nsIContent *content,
                                       const nsAString &sortResource,
                                       const nsAString &sortDirection)
{
  // set sort info on current column. This ensures that the
  // column header sort indicator is updated properly.
  PRUint32 numChildren = content->GetChildCount();

  for (PRUint32 childIndex = 0; childIndex < numChildren; ++childIndex) {
    nsIContent *child = content->GetChildAt(childIndex);

    if (child->IsNodeOfType(nsINode::eXUL)) {
      nsIAtom *tag = child->Tag();

      if (tag == nsGkAtoms::treecols) {
        SetSortColumnHints(child, sortResource, sortDirection);
      } else if (tag == nsGkAtoms::treecol) {
        nsAutoString value;
        child->GetAttr(kNameSpaceID_None, nsGkAtoms::sort, value);
        // also check the resource attribute for older code
        if (value.IsEmpty())
          child->GetAttr(kNameSpaceID_None, nsGkAtoms::resource, value);
        if (value == sortResource) {
          child->SetAttr(kNameSpaceID_None, nsGkAtoms::sortActive,
                         NS_LITERAL_STRING("true"), PR_TRUE);
          child->SetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection,
                         sortDirection, PR_TRUE);
          // Note: don't break out of loop; want to set/unset
          // attribs on ALL sort columns
        } else if (!value.IsEmpty()) {
          child->UnsetAttr(kNameSpaceID_None, nsGkAtoms::sortActive,
                           PR_TRUE);
          child->UnsetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection,
                           PR_TRUE);
        }
      }
    }
  }
}

nsresult
XULSortServiceImpl::GetItemsToSort(nsIContent *aContainer,
                                   nsSortState* aSortState,
                                   nsTArray<contentSortInfo>& aSortItems)
{
  // if there is a template attached to the sort node, use the builder to get
  // the items to be sorted
  nsCOMPtr<nsIDOMXULElement> element = do_QueryInterface(aContainer);
  if (element) {
    nsCOMPtr<nsIXULTemplateBuilder> builder;
    element->GetBuilder(getter_AddRefs(builder));

    if (builder) {
      nsresult rv = builder->GetQueryProcessor(getter_AddRefs(aSortState->processor));
      if (NS_FAILED(rv) || !aSortState->processor)
  return rv;

      return GetTemplateItemsToSort(aContainer, builder, aSortState, aSortItems);
    }
  }
  
  // if there is no template builder, just get the children. For trees,
  // get the treechildren element as use that as the parent
  nsCOMPtr<nsIContent> treechildren;
  if (aContainer->NodeInfo()->Equals(nsGkAtoms::tree, kNameSpaceID_XUL)) {
    nsXULContentUtils::FindChildByTag(aContainer,
                                      kNameSpaceID_XUL,
                                      nsGkAtoms::treechildren,
                                      getter_AddRefs(treechildren));
    if (!treechildren)
      return NS_OK;
  
    aContainer = treechildren;
    }

  PRUint32 count = aContainer->GetChildCount();
  for (PRUint32 c = 0; c < count; c++) {
    nsIContent *child = aContainer->GetChildAt(c);

    contentSortInfo* cinfo = aSortItems.AppendElement();
    if (!cinfo)
      return NS_ERROR_OUT_OF_MEMORY;

    cinfo->content = child;
      }

        return NS_OK;
}


nsresult
XULSortServiceImpl::GetTemplateItemsToSort(nsIContent* aContainer,
                                           nsIXULTemplateBuilder* aBuilder,
                                           nsSortState* aSortState,
                                           nsTArray<contentSortInfo>& aSortItems)
{
  PRUint32 numChildren = aContainer->GetChildCount();
  for (PRUint32 childIndex = 0; childIndex < numChildren; childIndex++) {
    nsIContent *child = aContainer->GetChildAt(childIndex);
  
    nsCOMPtr<nsIDOMElement> childnode = do_QueryInterface(child);

    nsCOMPtr<nsIXULTemplateResult> result;
    nsresult rv = aBuilder->GetResultForContent(childnode, getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);

    if (result) {
      contentSortInfo* cinfo = aSortItems.AppendElement();
      if (!cinfo)
        return NS_ERROR_OUT_OF_MEMORY;

      cinfo->content = child;
      cinfo->result = result;
      }
    else if (aContainer->Tag() != nsGkAtoms::_template) {
      rv = GetTemplateItemsToSort(child, aBuilder, aSortState, aSortItems);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

int PR_CALLBACK
testSortCallback(const void *data1, const void *data2, void *privateData)
{
  /// Note: testSortCallback is a small C callback stub for NS_QuickSort
  contentSortInfo *left = (contentSortInfo *)data1;
  contentSortInfo *right = (contentSortInfo *)data2;
  nsSortState* sortState = (nsSortState *)privateData;
      
  PRInt32 sortOrder = 0;

  if (sortState->direction == nsSortState_natural && sortState->processor) {
    // sort in natural order
    sortState->processor->CompareResults(left->result, right->result,
                                         nsnull, &sortOrder);
  }
  else {
    PRInt32 length = sortState->sortKeys.Count();
    for (PRInt32 t = 0; t < length; t++) {
      // for templates, use the query processor to do sorting
      if (sortState->processor) {
        sortState->processor->CompareResults(left->result, right->result,
                                             sortState->sortKeys[t], &sortOrder);
        if (sortOrder)
          break;
  }
      else {
        // no template, so just compare attributes. Ignore namespaces for now.
        nsAutoString leftstr, rightstr;
        left->content->GetAttr(kNameSpaceID_None, sortState->sortKeys[t], leftstr);
        right->content->GetAttr(kNameSpaceID_None, sortState->sortKeys[t], rightstr);

        if (!leftstr.Equals(rightstr)) {
          sortOrder = (leftstr > rightstr) ? 1 : -1;
          break;
    }
  }
    }
  }

  if (sortState->direction == nsSortState_descending)
    sortOrder = -sortOrder;

  return sortOrder;
}

nsresult
XULSortServiceImpl::SortContainer(nsIContent *aContainer, nsSortState* aSortState)
{
  nsTArray<contentSortInfo> items;
  nsresult rv = GetItemsToSort(aContainer, aSortState, items);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 numResults = items.Length();
  if (!numResults)
    return NS_OK;

  PRUint32 i;

  // inbetweenSeparatorSort sorts the items between separators independently
  if (aSortState->inbetweenSeparatorSort) {
    PRUint32 startIndex = 0;
    for (i = 0; i < numResults; i++) {
      if (i > startIndex + 1) {
        nsAutoString type;
        items[i].result->GetType(type);
        if (type.EqualsLiteral("separator")) {
          if (aSortState->invertSort)
            InvertSortInfo(items, startIndex, i - startIndex);
          else
            NS_QuickSort((void *)(items.Elements() + startIndex), i - startIndex,
                         sizeof(contentSortInfo), testSortCallback, (void*)aSortState);

          startIndex = i + 1;
      }
    }
  }
  
    if (i > startIndex + 1) {
      if (aSortState->invertSort)
        InvertSortInfo(items, startIndex, i - startIndex);
          else
        NS_QuickSort((void *)(items.Elements() + startIndex), i - startIndex,
                     sizeof(contentSortInfo), testSortCallback, (void*)aSortState);
      }
    } else {
    // if the items are just being inverted, that is, just switching between
    // ascending and descending, just reverse the list.
    if (aSortState->invertSort)
      InvertSortInfo(items, 0, numResults);
    else
      NS_QuickSort((void *)items.Elements(), numResults,
                   sizeof(contentSortInfo), testSortCallback, (void*)aSortState);
  }

  // first remove the items from the old positions
  for (i = 0; i < numResults; i++) {
    nsIContent* child = items[i].content;
    nsIContent* parent = child->GetParent();

    if (parent) {
      // remember the parent so that it can be reinserted back
      // into the same parent. This is necessary as multiple rules
      // may generate results which get placed in different locations.
      items[i].parent = parent;
      PRInt32 index = parent->IndexOf(child);
      parent->RemoveChildAt(index, PR_TRUE);
      }
    }

  // now add the items back in sorted order
  for (i = 0; i < numResults; i++)
  {
    nsIContent* child = items[i].content;
    nsIContent* parent = items[i].parent;
    if (parent) {
      parent->AppendChildTo(child, PR_TRUE);

      // if it's a container in a tree or menu, find its children,
      // and sort those also
      if (!child->AttrValueIs(kNameSpaceID_None, nsGkAtoms::container,
                              nsGkAtoms::_true, eCaseMatters))
        continue;
        
      PRUint32 numChildren = child->GetChildCount();
      for (PRUint32 gcindex = 0; gcindex < numChildren; gcindex++) {
        nsIContent *grandchild = child->GetChildAt(gcindex);
        nsINodeInfo *ni = grandchild->NodeInfo();
        nsIAtom *localName = ni->NameAtom();
        if (ni->NamespaceID() == kNameSpaceID_XUL &&
            (localName == nsGkAtoms::treechildren ||
             localName == nsGkAtoms::menupopup)) {
          SortContainer(grandchild, aSortState);
        }
      }
    }
  }
  
  return NS_OK;
}

nsresult
XULSortServiceImpl::InvertSortInfo(nsTArray<contentSortInfo>& aData,
                                   PRInt32 aStart, PRInt32 aNumItems)
{
  if (aNumItems > 1) {
    // reverse the items in the array starting from aStart
    PRInt32 upPoint = (aNumItems + 1) / 2 + aStart;
    PRInt32 downPoint = (aNumItems - 2) / 2 + aStart;
    PRInt32 half = aNumItems / 2;
    while (half-- > 0) {
      aData[downPoint--].swap(aData[upPoint++]);
    }
  }
  return NS_OK;
}

nsresult
XULSortServiceImpl::InitializeSortState(nsIContent* aRootElement,
                                        nsIContent* aContainer,
                                        const nsAString& aSortKey,
                                        const nsAString& aSortDirection,
                                        nsSortState* aSortState)
{
  // used as an optimization for the content builder
  if (aContainer != aSortState->lastContainer.get()) {
    aSortState->lastContainer = aContainer;
    aSortState->lastWasFirst = PR_FALSE;
    aSortState->lastWasLast = PR_FALSE;
  }

  // The attributes allowed are either:
  //    sort="key1 key2 ..."
  // or sortResource="key1" sortResource2="key2"
  // The latter is for backwards compatibility, and is equivalent to concatenating
  // both values in the sort attribute
  nsAutoString sort(aSortKey);
  aSortState->sortKeys.Clear();
  if (sort.IsEmpty()) {
    nsAutoString sortResource, sortResource2;
    aRootElement->GetAttr(kNameSpaceID_None, nsGkAtoms::sortResource, sortResource);
    if (!sortResource.IsEmpty()) {
      nsCOMPtr<nsIAtom> sortkeyatom = do_GetAtom(sortResource);
      aSortState->sortKeys.AppendObject(sortkeyatom);
      sort.Append(sortResource);

      aRootElement->GetAttr(kNameSpaceID_None, nsGkAtoms::sortResource2, sortResource2);
      if (!sortResource2.IsEmpty()) {
        nsCOMPtr<nsIAtom> sortkeyatom2 = do_GetAtom(sortResource2);
        aSortState->sortKeys.AppendObject(sortkeyatom2);
        sort.AppendLiteral(" ");
        sort.Append(sortResource2);
    }
  }
    }
    else {
    PRInt32 start = 0, end = 0;
    while ((end = sort.FindChar(' ',start)) >= 0) {
      if (end > start) {
        nsCOMPtr<nsIAtom> keyatom = do_GetAtom(Substring(sort, start, end - start));
        if (!keyatom)
          return NS_ERROR_OUT_OF_MEMORY;

        aSortState->sortKeys.AppendObject(keyatom);
      }
      start = end + 1;
    }
    if (start < (PRInt32)sort.Length()) {
      nsCOMPtr<nsIAtom> keyatom = do_GetAtom(Substring(sort, start));
      if (!keyatom)
        return NS_ERROR_OUT_OF_MEMORY;

      aSortState->sortKeys.AppendObject(keyatom);
    }
  }

  aSortState->sort.Assign(sort);

  // set up sort order info
  if (aSortDirection.EqualsLiteral("descending"))
    aSortState->direction = nsSortState_descending;
  else if (aSortDirection.EqualsLiteral("ascending"))
    aSortState->direction = nsSortState_ascending;
        else
    aSortState->direction = nsSortState_natural;

  aSortState->invertSort = PR_FALSE;
          
  nsAutoString existingsort;
  aRootElement->GetAttr(kNameSpaceID_None, nsGkAtoms::sort, existingsort);
  nsAutoString existingsortDirection;
  aRootElement->GetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection, existingsortDirection);
          
  // if just switching direction, set the invertSort flag
  if (sort.Equals(existingsort)) {
    if (aSortState->direction == nsSortState_descending) {
      if (existingsortDirection.EqualsLiteral("ascending"))
        aSortState->invertSort = PR_TRUE;
      }
    else if (aSortState->direction == nsSortState_ascending &&
              existingsortDirection.EqualsLiteral("descending")) {
      aSortState->invertSort = PR_TRUE;
    }
  }

  // sort items between separatore independently
  aSortState->inbetweenSeparatorSort =
    aRootElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::sortSeparators,
                              nsGkAtoms::_true, eCaseMatters);

  // sort static content (non template generated nodes) after generated content
  aSortState->sortStaticsLast = aRootElement->AttrValueIs(kNameSpaceID_None,
                                  nsGkAtoms::sortStaticsLast,
                                  nsGkAtoms::_true, eCaseMatters);

  aSortState->initialized = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
XULSortServiceImpl::Sort(nsIDOMNode* aNode,
                         const nsAString& aSortKey,
                         const nsAString& aSortDirection)
{
  // get root content node
  nsCOMPtr<nsIContent> sortNode = do_QueryInterface(aNode);
  if (!sortNode)
    return NS_ERROR_FAILURE;

  nsSortState sortState;
  nsresult rv = InitializeSortState(sortNode, sortNode,
                                    aSortKey, aSortDirection, &sortState);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // store sort info in attributes on content
  SetSortHints(sortNode, &sortState);
  rv = SortContainer(sortNode, &sortState);
  
  sortState.processor = nsnull; // don't hang on to this reference
  return rv;
}

nsresult
NS_NewXULSortService(nsIXULSortService** sortService)
{
  *sortService = new XULSortServiceImpl();
  if (!*sortService)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*sortService);
  return NS_OK;
}
