/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  This sort service is used to sort content by attribute.
 */

#ifndef nsXULSortService_h
#define nsXULSortService_h

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsIContent.h"
#include "nsIXULSortService.h"
#include "nsCycleCollectionParticipant.h"

enum nsSortState_direction {
  nsSortState_descending,
  nsSortState_ascending,
  nsSortState_natural
};

// the sort state holds info about the current sort
struct nsSortState
{
  bool initialized;
  MOZ_INIT_OUTSIDE_CTOR bool invertSort;

  uint32_t sortHints;

  MOZ_INIT_OUTSIDE_CTOR nsSortState_direction direction;
  nsAutoString sort;
  nsTArray<RefPtr<nsAtom>> sortKeys;

  nsCOMPtr<nsIContent> lastContainer;
  MOZ_INIT_OUTSIDE_CTOR bool lastWasFirst, lastWasLast;

  nsSortState()
    : initialized(false),
      sortHints(0)
  {
  }
  void Traverse(nsCycleCollectionTraversalCallback &cb) const
  {
    cb.NoteXPCOMChild(lastContainer);
  }
};

// information about a particular item to be sorted
struct contentSortInfo {
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> parent;
  void swap(contentSortInfo& other)
  {
    content.swap(other.content);
    parent.swap(other.parent);
  }
};

////////////////////////////////////////////////////////////////////////
// ServiceImpl
//
//   This is the sort service.
//
class XULSortServiceImpl : public nsIXULSortService
{
protected:
  XULSortServiceImpl(void) {}
  virtual ~XULSortServiceImpl(void) {}

  friend nsresult NS_NewXULSortService(nsIXULSortService** mgr);

private:

public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsISortService
  NS_DECL_NSIXULSORTSERVICE

  /**
   * Set sort and sortDirection attributes when a sort is done.
   */
  void
  SetSortHints(mozilla::dom::Element* aElement, nsSortState* aSortState);

  /**
   * Set sortActive and sortDirection attributes on a tree column when a sort
   * is done. The column to change is the one with a sort attribute that
   * matches the sort key. The sort attributes are removed from the other
   * columns.
   */
  void
  SetSortColumnHints(nsIContent *content,
                     const nsAString &sortResource,
                     const nsAString &sortDirection);

  /**
   * Determine the list of items which need to be sorted. This is determined
   * in the following way:
   *   - for elements that have a content builder, get its list of generated
   *     results
   *   - otherwise, for trees, get the child treeitems
   *   - otherwise, get the direct children
   */
  nsresult
  GetItemsToSort(nsIContent *aContainer,
                 nsSortState* aSortState,
                 nsTArray<contentSortInfo>& aSortItems);

  /**
   * Sort a container using the supplied sort state details.
   */
  nsresult
  SortContainer(nsIContent *aContainer, nsSortState* aSortState);

  /**
   * Given a list of sortable items, reverse the list. This is done
   * when simply changing the sort direction for the same key.
   */
  nsresult
  InvertSortInfo(nsTArray<contentSortInfo>& aData,
                 int32_t aStart, int32_t aNumItems);

  /**
   * Initialize sort information from attributes specified on the container,
   * the sort key and sort direction.
   *
   * @param aRootElement the element that contains sort attributes
   * @param aContainer the container to sort, usually equal to aRootElement
   * @param aSortKey space separated list of sort keys
   * @param aSortDirection direction to sort in
   * @param aSortState structure filled in with sort data
   */
  static nsresult
  InitializeSortState(mozilla::dom::Element* aRootElement,
                      mozilla::dom::Element* aContainer,
                      const nsAString& aSortKey,
                      const nsAString& aSortDirection,
                      nsSortState* aSortState);

  /**
   * Compares aLeft and aRight and returns < 0, 0, or > 0. The sort
   * hints are checked for case matching and integer sorting.
   */
  static int32_t CompareValues(const nsAString& aLeft,
                               const nsAString& aRight,
                               uint32_t aSortHints);
};

#endif // nsXULSortService_h
