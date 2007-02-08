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
  This sort service is used to sort template built content or content by attribute.
 */

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsIContent.h"
#include "nsIXULTemplateResult.h"
#include "nsIXULTemplateQueryProcessor.h"
#include "nsIXULSortService.h"

enum nsSortState_direction {
  nsSortState_descending,
  nsSortState_ascending,
  nsSortState_natural
};
  
// the sort state holds info about the current sort
struct nsSortState
{
  PRBool initialized;
  PRBool invertSort;
  PRBool inbetweenSeparatorSort;
  PRBool sortStaticsLast;
  PRBool isContainerRDFSeq;

  nsSortState_direction direction;
  nsAutoString sort;
  nsCOMArray<nsIAtom> sortKeys;

  nsCOMPtr<nsIXULTemplateQueryProcessor> processor;
  nsCOMPtr<nsIContent> lastContainer;
  PRBool lastWasFirst, lastWasLast;

  nsSortState()
    : initialized(PR_FALSE)
  {
  }
};

// information about a particular item to be sorted
struct contentSortInfo {
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> parent;
  nsCOMPtr<nsIXULTemplateResult> result;
  void swap(contentSortInfo& other)
  {
    content.swap(other.content);
    parent.swap(other.parent);
    result.swap(other.result);
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
  XULSortServiceImpl(void) {};
  virtual ~XULSortServiceImpl(void) {};

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
  SetSortHints(nsIContent *aNode, nsSortState* aSortState);

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
   * Get the list of items to sort for template built content
   */
  nsresult
  GetTemplateItemsToSort(nsIContent* aContainer,
                         nsIXULTemplateBuilder* aBuilder,
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
                 PRInt32 aStart, PRInt32 aNumItems);

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
  InitializeSortState(nsIContent* aRootElement,
                      nsIContent* aContainer,
                      const nsAString& aSortKey,
                      const nsAString& aSortDirection,
                      nsSortState* aSortState);
};
