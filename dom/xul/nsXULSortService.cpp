/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  This file provides the implementation for the sort service manager.
 */

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsXULContentUtils.h"
#include "nsString.h"
#include "nsQuickSort.h"
#include "nsWhitespaceTokenizer.h"
#include "nsXULSortService.h"
#include "nsXULElement.h"
#include "nsICollation.h"
#include "nsUnicharUtils.h"

NS_IMPL_ISUPPORTS(XULSortServiceImpl, nsIXULSortService)

void
XULSortServiceImpl::SetSortHints(Element* aElement, nsSortState* aSortState)
{
  // set sort and sortDirection attributes when is sort is done
  aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::sort,
                    aSortState->sort, true);

  nsAutoString direction;
  if (aSortState->direction == nsSortState_descending)
    direction.AssignLiteral("descending");
  else if (aSortState->direction == nsSortState_ascending)
    direction.AssignLiteral("ascending");
  aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection,
                    direction, true);

  // for trees, also set the sort info on the currently sorted column
  if (aElement->NodeInfo()->Equals(nsGkAtoms::tree, kNameSpaceID_XUL)) {
    if (aSortState->sortKeys.Length() >= 1) {
      nsAutoString sortkey;
      aSortState->sortKeys[0]->ToString(sortkey);
      SetSortColumnHints(aElement, sortkey, direction);
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
    if (child->IsXULElement(nsGkAtoms::treecols)) {
      SetSortColumnHints(child, sortResource, sortDirection);
    } else if (child->IsXULElement(nsGkAtoms::treecol)) {
      nsAutoString value;
      child->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::sort, value);
      if (value == sortResource) {
        child->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::sortActive,
                                    NS_LITERAL_STRING("true"), true);

        child->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection,
                                    sortDirection, true);
        // Note: don't break out of loop; want to set/unset
        // attribs on ALL sort columns
      } else if (!value.IsEmpty()) {
        child->AsElement()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::sortActive,
                                      true);
        child->AsElement()->UnsetAttr(kNameSpaceID_None,
                                      nsGkAtoms::sortDirection, true);
      }
    }
  }
}

nsresult
XULSortServiceImpl::GetItemsToSort(nsIContent *aContainer,
                                   nsSortState* aSortState,
                                   nsTArray<contentSortInfo>& aSortItems)
{
  // Get the children. For trees, get the treechildren element and
  // use that as the parent
  RefPtr<Element> treechildren;
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


int
testSortCallback(const void *data1, const void *data2, void *privateData)
{
  /// Note: testSortCallback is a small C callback stub for NS_QuickSort
  contentSortInfo *left = (contentSortInfo *)data1;
  contentSortInfo *right = (contentSortInfo *)data2;
  nsSortState* sortState = (nsSortState *)privateData;

  int32_t sortOrder = 0;

  int32_t length = sortState->sortKeys.Length();
  for (int32_t t = 0; t < length; t++) {
    // compare attributes. Ignore namespaces for now.
    nsAutoString leftstr, rightstr;
    if (left->content->IsElement()) {
      left->content->AsElement()->GetAttr(kNameSpaceID_None,
                                          sortState->sortKeys[t],
                                          leftstr);
    }
    if (right->content->IsElement()) {
      right->content->AsElement()->GetAttr(kNameSpaceID_None,
                                           sortState->sortKeys[t], rightstr);
    }

    sortOrder = XULSortServiceImpl::CompareValues(leftstr, rightstr, sortState->sortHints);
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

  uint32_t numResults = items.Length();
  if (!numResults)
    return NS_OK;

  uint32_t i;

  // if the items are just being inverted, that is, just switching between
  // ascending and descending, just reverse the list.
  if (aSortState->invertSort)
    InvertSortInfo(items, 0, numResults);
  else
    NS_QuickSort((void *)items.Elements(), numResults,
                 sizeof(contentSortInfo), testSortCallback, (void*)aSortState);

  // first remove the items from the old positions
  for (i = 0; i < numResults; i++) {
    nsIContent* child = items[i].content;
    nsIContent* parent = child->GetParent();

    if (parent) {
      // remember the parent so that it can be reinserted back
      // into the same parent. This is necessary as multiple rules
      // may generate results which get placed in different locations.
      items[i].parent = parent;
      parent->RemoveChildNode(child, true);
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
      if (!child->IsElement() ||
          !child->AsElement()->AttrValueIs(kNameSpaceID_None,
                                           nsGkAtoms::container,
                                           nsGkAtoms::_true, eCaseMatters))
        continue;

      for (nsIContent* grandchild = child->GetFirstChild();
           grandchild;
           grandchild = grandchild->GetNextSibling()) {
        mozilla::dom::NodeInfo *ni = grandchild->NodeInfo();
        nsAtom *localName = ni->NameAtom();
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
                                   int32_t aStart, int32_t aNumItems)
{
  if (aNumItems > 1) {
    // reverse the items in the array starting from aStart
    int32_t upPoint = (aNumItems + 1) / 2 + aStart;
    int32_t downPoint = (aNumItems - 2) / 2 + aStart;
    int32_t half = aNumItems / 2;
    while (half-- > 0) {
      aData[downPoint--].swap(aData[upPoint++]);
    }
  }
  return NS_OK;
}

nsresult
XULSortServiceImpl::InitializeSortState(Element* aRootElement,
                                        Element* aContainer,
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

  // The sort attribute is of the form: sort="key1 key2 ..."
  nsAutoString sort(aSortKey);
  aSortState->sortKeys.Clear();
  nsWhitespaceTokenizer tokenizer(sort);
  while (tokenizer.hasMoreTokens()) {
    RefPtr<nsAtom> keyatom = NS_Atomize(tokenizer.nextToken());
    NS_ENSURE_TRUE(keyatom, NS_ERROR_OUT_OF_MEMORY);
    aSortState->sortKeys.AppendElement(keyatom);
  }

  aSortState->sort.Assign(sort);
  aSortState->direction = nsSortState_natural;

  bool noNaturalState = false;
  nsWhitespaceTokenizer hintsTokenizer(aSortHints);
  while (hintsTokenizer.hasMoreTokens()) {
    const nsDependentSubstring& token(hintsTokenizer.nextToken());
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

  aSortState->initialized = true;

  return NS_OK;
}

int32_t
XULSortServiceImpl::CompareValues(const nsAString& aLeft,
                                  const nsAString& aRight,
                                  uint32_t aSortHints)
{
  if (aSortHints & SORT_INTEGER) {
    nsresult err;
    int32_t leftint = PromiseFlatString(aLeft).ToInteger(&err);
    if (NS_SUCCEEDED(err)) {
      int32_t rightint = PromiseFlatString(aRight).ToInteger(&err);
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
    int32_t result;
    collation->CompareString(nsICollation::kCollationCaseInSensitive,
                             aLeft, aRight, &result);
    return result;
  }

  return ::Compare(aLeft, aRight, nsCaseInsensitiveStringComparator());
}

NS_IMETHODIMP
XULSortServiceImpl::Sort(Element* aNode,
                         const nsAString& aSortKey,
                         const nsAString& aSortHints)
{
  nsSortState sortState;
  nsresult rv = InitializeSortState(aNode, aNode,
                                    aSortKey, aSortHints, &sortState);
  NS_ENSURE_SUCCESS(rv, rv);

  // store sort info in attributes on content
  SetSortHints(aNode, &sortState);
  rv = SortContainer(aNode, &sortState);

  return rv;
}

nsresult
NS_NewXULSortService(nsIXULSortService** sortService)
{
  *sortService = new XULSortServiceImpl();
  NS_ADDREF(*sortService);
  return NS_OK;
}
