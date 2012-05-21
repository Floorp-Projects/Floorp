/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
#include "nsWhitespaceTokenizer.h"
#include "nsXULSortService.h"
#include "nsIDOMXULElement.h"
#include "nsIXULTemplateBuilder.h"
#include "nsTemplateMatch.h"
#include "nsICollation.h"
#include "nsUnicharUtils.h"

NS_IMPL_ISUPPORTS1(XULSortServiceImpl, nsIXULSortService)

void
XULSortServiceImpl::SetSortHints(nsIContent *aNode, nsSortState* aSortState)
{
  // set sort and sortDirection attributes when is sort is done
  aNode->SetAttr(kNameSpaceID_None, nsGkAtoms::sort,
                 aSortState->sort, true);

  nsAutoString direction;
  if (aSortState->direction == nsSortState_descending)
    direction.AssignLiteral("descending");
  else if (aSortState->direction == nsSortState_ascending)
    direction.AssignLiteral("ascending");
  aNode->SetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection,
                 direction, true);

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
  for (nsIContent* child = content->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsXUL()) {
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
                         NS_LITERAL_STRING("true"), true);
          child->SetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection,
                         sortDirection, true);
          // Note: don't break out of loop; want to set/unset
          // attribs on ALL sort columns
        } else if (!value.IsEmpty()) {
          child->UnsetAttr(kNameSpaceID_None, nsGkAtoms::sortActive,
                           true);
          child->UnsetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection,
                           true);
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
  
  for (nsIContent* child = aContainer->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
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
  for (nsIContent* child = aContainer->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
  
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

int
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
                                         nsnull, sortState->sortHints, &sortOrder);
  }
  else {
    PRInt32 length = sortState->sortKeys.Count();
    for (PRInt32 t = 0; t < length; t++) {
      // for templates, use the query processor to do sorting
      if (sortState->processor) {
        sortState->processor->CompareResults(left->result, right->result,
                                             sortState->sortKeys[t],
                                             sortState->sortHints, &sortOrder);
        if (sortOrder)
          break;
      }
      else {
        // no template, so just compare attributes. Ignore namespaces for now.
        nsAutoString leftstr, rightstr;
        left->content->GetAttr(kNameSpaceID_None, sortState->sortKeys[t], leftstr);
        right->content->GetAttr(kNameSpaceID_None, sortState->sortKeys[t], rightstr);

        sortOrder = XULSortServiceImpl::CompareValues(leftstr, rightstr, sortState->sortHints);
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
      parent->RemoveChildAt(index, true);
    }
  }

  // now add the items back in sorted order
  for (i = 0; i < numResults; i++)
  {
    nsIContent* child = items[i].content;
    nsIContent* parent = items[i].parent;
    if (parent) {
      parent->AppendChildTo(child, true);

      // if it's a container in a tree or menu, find its children,
      // and sort those also
      if (!child->AttrValueIs(kNameSpaceID_None, nsGkAtoms::container,
                              nsGkAtoms::_true, eCaseMatters))
        continue;
        
      for (nsIContent* grandchild = child->GetFirstChild();
           grandchild;
           grandchild = grandchild->GetNextSibling()) {
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
                                        const nsAString& aSortHints,
                                        nsSortState* aSortState)
{
  // used as an optimization for the content builder
  if (aContainer != aSortState->lastContainer.get()) {
    aSortState->lastContainer = aContainer;
    aSortState->lastWasFirst = false;
    aSortState->lastWasLast = false;
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
    nsWhitespaceTokenizer tokenizer(sort);
    while (tokenizer.hasMoreTokens()) {
      nsCOMPtr<nsIAtom> keyatom = do_GetAtom(tokenizer.nextToken());
      NS_ENSURE_TRUE(keyatom, NS_ERROR_OUT_OF_MEMORY);
      aSortState->sortKeys.AppendObject(keyatom);
    }
  }

  aSortState->sort.Assign(sort);
  aSortState->direction = nsSortState_natural;

  bool noNaturalState = false;
  nsWhitespaceTokenizer tokenizer(aSortHints);
  while (tokenizer.hasMoreTokens()) {
    const nsDependentSubstring& token(tokenizer.nextToken());
    if (token.EqualsLiteral("comparecase"))
      aSortState->sortHints |= nsIXULSortService::SORT_COMPARECASE;
    else if (token.EqualsLiteral("integer"))
      aSortState->sortHints |= nsIXULSortService::SORT_INTEGER;
    else if (token.EqualsLiteral("descending"))
      aSortState->direction = nsSortState_descending;
    else if (token.EqualsLiteral("ascending"))
      aSortState->direction = nsSortState_ascending;
    else if (token.EqualsLiteral("twostate"))
      noNaturalState = true;
  }

  // if the twostate flag was set, the natural order is skipped and only
  // ascending and descending are allowed
  if (aSortState->direction == nsSortState_natural && noNaturalState) {
    aSortState->direction = nsSortState_ascending;
  }

  // set up sort order info
  aSortState->invertSort = false;

  nsAutoString existingsort;
  aRootElement->GetAttr(kNameSpaceID_None, nsGkAtoms::sort, existingsort);
  nsAutoString existingsortDirection;
  aRootElement->GetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection, existingsortDirection);
          
  // if just switching direction, set the invertSort flag
  if (sort.Equals(existingsort)) {
    if (aSortState->direction == nsSortState_descending) {
      if (existingsortDirection.EqualsLiteral("ascending"))
        aSortState->invertSort = true;
    }
    else if (aSortState->direction == nsSortState_ascending &&
             existingsortDirection.EqualsLiteral("descending")) {
      aSortState->invertSort = true;
    }
  }

  // sort items between separators independently
  aSortState->inbetweenSeparatorSort =
    aRootElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::sortSeparators,
                              nsGkAtoms::_true, eCaseMatters);

  // sort static content (non template generated nodes) after generated content
  aSortState->sortStaticsLast = aRootElement->AttrValueIs(kNameSpaceID_None,
                                  nsGkAtoms::sortStaticsLast,
                                  nsGkAtoms::_true, eCaseMatters);

  aSortState->initialized = true;

  return NS_OK;
}

PRInt32
XULSortServiceImpl::CompareValues(const nsAString& aLeft,
                                  const nsAString& aRight,
                                  PRUint32 aSortHints)
{
  if (aSortHints & SORT_INTEGER) {
    PRInt32 err;
    PRInt32 leftint = PromiseFlatString(aLeft).ToInteger(&err);
    if (NS_SUCCEEDED(err)) {
      PRInt32 rightint = PromiseFlatString(aRight).ToInteger(&err);
      if (NS_SUCCEEDED(err)) {
        return leftint - rightint;
      }
    }
    // if they aren't integers, just fall through and compare strings
  }

  if (aSortHints & SORT_COMPARECASE) {
    return ::Compare(aLeft, aRight);
  }

  nsICollation* collation = nsXULContentUtils::GetCollation();
  if (collation) {
    PRInt32 result;
    collation->CompareString(nsICollation::kCollationCaseInSensitive,
                             aLeft, aRight, &result);
    return result;
  }

  return ::Compare(aLeft, aRight, nsCaseInsensitiveStringComparator());
}

NS_IMETHODIMP
XULSortServiceImpl::Sort(nsIDOMNode* aNode,
                         const nsAString& aSortKey,
                         const nsAString& aSortHints)
{
  // get root content node
  nsCOMPtr<nsIContent> sortNode = do_QueryInterface(aNode);
  if (!sortNode)
    return NS_ERROR_FAILURE;

  nsSortState sortState;
  nsresult rv = InitializeSortState(sortNode, sortNode,
                                    aSortKey, aSortHints, &sortState);
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
