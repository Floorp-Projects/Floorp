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
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsRDFCID.h"
#include "nsXULContentUtils.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsUnicharUtils.h"
#include "rdf.h"
#include "nsRDFSort.h"
#include "nsVoidArray.h"
#include "nsQuickSort.h"
#include "nsIXULSortService.h"
#include "prlog.h"
#include "nsICollation.h"
#include "nsCollationCID.h"
#include "nsLayoutCID.h"
#include "nsIDOMXULElement.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsIRDFContainerUtils.h"
#include "nsXULAtoms.h"
#include "nsINodeInfo.h"


////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kRDFServiceCID,          NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kCollationFactoryCID,    NS_COLLATIONFACTORY_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,   NS_RDFCONTAINERUTILS_CID);

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, BookmarkSeparator);

typedef struct  _sortStruct  {
  PRBool firstFlag;
  nsCOMPtr<nsIRDFResource> sortProperty, sortProperty2;
  nsCOMPtr<nsIRDFResource> sortPropertyColl, sortPropertyColl2;
  nsCOMPtr<nsIRDFResource> sortPropertySort, sortPropertySort2;

  PRBool cacheFirstHint;
  nsCOMPtr<nsIRDFNode> cacheFirstNode;
  PRBool cacheIsFirstNodeCollationKey;

  nsCOMPtr<nsIRDFCompositeDataSource> db;
  nsCOMPtr<nsIRDFDataSource> mInner;
  nsCOMPtr<nsIContent> parentContainer;
  PRBool descendingSort;
  PRBool naturalOrderSort;
  PRBool inbetweenSeparatorSort;

} sortStruct, *sortPtr;

typedef struct {
  nsIContent * content;
  nsCOMPtr<nsIRDFResource> resource; 
  nsCOMPtr<nsIRDFNode> collationNode1;
  nsCOMPtr<nsIRDFNode> collationNode2;
  nsCOMPtr<nsIRDFNode> sortNode1;
  nsCOMPtr<nsIRDFNode> sortNode2;
  nsCOMPtr<nsIRDFNode> node1;
  nsCOMPtr<nsIRDFNode> node2;
  PRBool checkedCollation1;
  PRBool checkedCollation2;
  PRBool checkedSort1;
  PRBool checkedSort2;
  PRBool checkedNode1;
  PRBool checkedNode2;
} contentSortInfo;

int PR_CALLBACK inplaceSortCallback(const void *data1, const void *data2, void *privateData);
int PR_CALLBACK testSortCallback(const void * data1, const void *data2, void *privateData);

////////////////////////////////////////////////////////////////////////
// ServiceImpl
//
//   This is the sort service.
//
class XULSortServiceImpl : public nsIXULSortService
{
protected:
  XULSortServiceImpl(void);
  virtual ~XULSortServiceImpl(void);

  static nsICollation *gCollation;

  friend nsresult NS_NewXULSortService(nsIXULSortService** mgr);

private:
  static nsrefcnt gRefCnt;
  static nsString *kTrueStr;
  static nsString *kNaturalStr;
  static nsString *kAscendingStr;
  static nsString *kDescendingStr;

  static nsIRDFService *gRDFService;
  static nsIRDFContainerUtils *gRDFC;

nsresult FindDatabaseElement(nsIContent* aElement, nsIContent** aDatabaseElement);
nsresult FindSortableContainer(nsIContent *tree, nsIContent **treeBody);
nsresult SetSortHints(nsIContent *tree, const nsAString &sortResource, const nsAString &sortDirection, const nsAString &sortResource2, PRBool inbetweenSeparatorSort, PRBool found);
nsresult SetSortColumnHints(nsIContent *content, const nsAString &sortResource, const nsAString &sortDirection);
nsresult GetSortColumnInfo(nsIContent *tree, nsAString &sortResource, nsAString &sortDirection, nsAString &sortResource2, PRBool &inbetweenSeparatorSort);

nsresult SortContainer(nsIContent *container, sortPtr sortInfo, PRBool merelyInvertFlag);
nsresult InvertSortInfo(contentSortInfo **data, PRInt32 numItems);

static nsresult  GetCachedTarget(sortPtr sortInfo, PRBool useCache, nsIRDFResource* aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **aResult);
static nsresult GetTarget(contentSortInfo *contentSortInfo, sortPtr sortInfo,  PRBool first, PRBool onlyCollationHint, PRBool truthValue,nsIRDFNode **target, PRBool &isCollationKey);
static nsresult GetResourceValue(nsIRDFResource *res1, sortPtr sortInfo, PRBool first, PRBool useCache, PRBool onlyCollationHint, nsIRDFNode **, PRBool &isCollationKey);
static nsresult GetResourceValue(contentSortInfo *contentSortInfo, sortPtr sortInfo, PRBool first, PRBool useCache,  PRBool onlyCollationHint, nsIRDFNode **target, PRBool &isCollationKey);
static nsresult GetNodeValue(nsIContent *node1, sortPtr sortInfo, PRBool first, PRBool onlyCollationHint, nsIRDFNode **, PRBool &isCollationKey);
static nsresult GetNodeValue(contentSortInfo *info1, sortPtr sortInfo, PRBool first, PRBool onlyCollationHint, nsIRDFNode **theNode, PRBool &isCollationKey);

public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsISortService
  NS_DECL_NSIXULSORTSERVICE

  static nsresult InplaceSort(nsIContent *node1, nsIContent *node2, sortPtr sortInfo, PRInt32 & sortOrder);
  static nsresult InplaceSort(contentSortInfo *info1, contentSortInfo *info2, sortPtr sortInfo, PRInt32 & sortOrder);
  static nsresult CompareNodes(nsIRDFNode *cellNode1, PRBool isCollationKey1,
                               nsIRDFNode *cellNode2, PRBool isCollationKey2,
                               PRBool &bothValid, PRInt32 & sortOrder);
};

nsICollation *XULSortServiceImpl::gCollation = nsnull;
nsIRDFService *XULSortServiceImpl::gRDFService = nsnull;
nsIRDFContainerUtils *XULSortServiceImpl::gRDFC = nsnull;
nsrefcnt XULSortServiceImpl::gRefCnt = 0;

nsString* XULSortServiceImpl::kTrueStr = nsnull;
nsString* XULSortServiceImpl::kNaturalStr = nsnull;
nsString* XULSortServiceImpl::kAscendingStr = nsnull;
nsString* XULSortServiceImpl::kDescendingStr = nsnull;

////////////////////////////////////////////////////////////////////////

XULSortServiceImpl::XULSortServiceImpl(void)
{
  if (gRefCnt == 0) {
    kTrueStr = new nsString(NS_LITERAL_STRING("true"));
    kNaturalStr = new nsString(NS_LITERAL_STRING("natural"));
    kAscendingStr = new nsString(NS_LITERAL_STRING("ascending"));
    kDescendingStr = new nsString(NS_LITERAL_STRING("descending"));
 
    nsresult rv = CallGetService(kRDFServiceCID, &gRDFService);

    NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't create rdf service");

    rv = CallGetService(kRDFContainerUtilsCID, &gRDFC);

    NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't create rdf container utils");

    // get a locale service 
    nsCOMPtr<nsILocaleService> localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsILocale>  locale;
      if (NS_SUCCEEDED(rv = localeService->GetApplicationLocale(getter_AddRefs(locale))) && (locale)) {
        nsCOMPtr<nsICollationFactory> colFactory = do_CreateInstance(kCollationFactoryCID);
        if (colFactory) {
          rv = colFactory->CreateCollation(locale, &gCollation);

          NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't create collation instance");
        } else
          NS_ERROR("couldn't create instance of collation factory");
      } else
        NS_ERROR("unable to get application locale");
    } else
      NS_ERROR("couldn't get locale factory");
  }

  ++gRefCnt;
}

XULSortServiceImpl::~XULSortServiceImpl(void) {
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: XULSortServiceImpl\n", gInstanceCount);
#endif

  --gRefCnt;
  if (gRefCnt == 0) {
    delete kTrueStr;
    kTrueStr = nsnull;
    delete kAscendingStr;
    kAscendingStr = nsnull;
    delete kDescendingStr;
    kDescendingStr = nsnull;
    delete kNaturalStr;
    kNaturalStr = nsnull;

    NS_IF_RELEASE(gCollation);

    NS_IF_RELEASE(gRDFService);
    NS_IF_RELEASE(gRDFC);
  }
}

NS_IMPL_ISUPPORTS1(XULSortServiceImpl, nsIXULSortService)

////////////////////////////////////////////////////////////////////////

nsresult
XULSortServiceImpl::FindDatabaseElement(nsIContent *aElement, nsIContent **aDatabaseElement)
{
  *aDatabaseElement = nsnull;

  // so look from the current node upwards until we find a node with a database
  for (nsIContent* content = aElement; content; content = content->GetParent()) {
    nsCOMPtr<nsIDOMXULElement> element = do_QueryInterface(content);
    nsCOMPtr<nsIRDFCompositeDataSource> db;
    element->GetDatabase(getter_AddRefs(db));
    if (db) {
      NS_ADDREF(*aDatabaseElement = content);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult
XULSortServiceImpl::FindSortableContainer(nsIContent *aRoot,
                                          nsIContent **aContainer)
{
  *aContainer = nsnull;

  nsresult rv;

  nsIAtom *tag = aRoot->Tag();

  if (aRoot->IsContentOfType(nsIContent::eXUL)) {
    if (tag == nsXULAtoms::templateAtom) // ignore content within templates
      return NS_OK;    

    if (tag == nsXULAtoms::listbox ||
        tag == nsXULAtoms::treechildren ||
        tag == nsXULAtoms::menupopup) {
      *aContainer = aRoot;
      NS_ADDREF(*aContainer);

      return NS_OK;
    }
  }

  PRUint32 numChildren = aRoot->GetChildCount();

  for (PRUint32 childIndex = 0; childIndex < numChildren; childIndex++) {
    nsIContent *child = aRoot->GetChildAt(childIndex);

    if (child->IsContentOfType(nsIContent::eXUL)) {
      rv = FindSortableContainer(child, aContainer);
      if (*aContainer)
        return rv;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult
XULSortServiceImpl::SetSortHints(nsIContent *tree,
                                 const nsAString &sortResource,
                                 const nsAString &sortDirection,
                                 const nsAString &sortResource2,
                                 PRBool inbetweenSeparatorSort, PRBool found)
{
  if (found) {
    // set hints on tree root node
    tree->SetAttr(kNameSpaceID_None, nsXULAtoms::sortActive, *kTrueStr, PR_FALSE);
    tree->SetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, sortDirection, PR_FALSE);
    tree->SetAttr(kNameSpaceID_None, nsXULAtoms::sortResource, sortResource, PR_FALSE);

    if (!sortResource2.IsEmpty())
      tree->SetAttr(kNameSpaceID_None, nsXULAtoms::sortResource2, sortResource2, PR_FALSE);
    else
      tree->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortResource2, PR_FALSE);
  } else {
    tree->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortActive, PR_FALSE);
    tree->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, PR_FALSE);
    tree->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortResource, PR_FALSE);
    tree->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortResource2, PR_FALSE);
  }

  // optional hint
  if (inbetweenSeparatorSort == PR_TRUE)
    tree->SetAttr(kNameSpaceID_None, nsXULAtoms::sortSeparators, *kTrueStr, PR_FALSE);
  else
    tree->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortSeparators, PR_FALSE);

  SetSortColumnHints(tree, sortResource, sortDirection);

  return NS_OK;
}

nsresult
XULSortServiceImpl::SetSortColumnHints(nsIContent *content,
                                       const nsAString &sortResource,
                                       const nsAString &sortDirection)
{
  nsresult rv;

  PRUint32 numChildren = content->GetChildCount();

  for (PRUint32 childIndex = 0; childIndex < numChildren; ++childIndex) {
    nsIContent *child = content->GetChildAt(childIndex);

    if (child->IsContentOfType(nsIContent::eXUL)) {
      nsIAtom *tag = child->Tag();

      if (tag == nsXULAtoms::treecols ||
          tag == nsXULAtoms::listcols ||
          tag == nsXULAtoms::listhead) {
        rv = SetSortColumnHints(child, sortResource, sortDirection);
      } else if (tag == nsXULAtoms::treecol ||
                 tag == nsXULAtoms::listcol ||
                 tag == nsXULAtoms::listheader) {
        nsAutoString value;

        if (NS_SUCCEEDED(rv = child->GetAttr(kNameSpaceID_None,
                                             nsXULAtoms::resource, value))
            && rv == NS_CONTENT_ATTR_HAS_VALUE)
        {
          if (value == sortResource) {
            child->SetAttr(kNameSpaceID_None, nsXULAtoms::sortActive,
                           *kTrueStr, PR_TRUE);
            child->SetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection,
                           sortDirection, PR_TRUE);
            // Note: don't break out of loop; want to set/unset
            // attribs on ALL sort columns
          } else {
            child->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortActive,
                             PR_TRUE);
            child->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection,
                             PR_TRUE);
          }
        }
      }
    }
  }
  
  return NS_OK;
}

nsresult
XULSortServiceImpl::GetSortColumnInfo(nsIContent *tree,
                                      nsAString &sortResource,
                                      nsAString &sortDirection,
                                      nsAString &sortResource2,
                                      PRBool &inbetweenSeparatorSort)
{
  nsresult rv = NS_ERROR_FAILURE;
  inbetweenSeparatorSort = PR_FALSE;

  nsAutoString value;
  if (NS_SUCCEEDED(rv = tree->GetAttr(kNameSpaceID_None,
                                      nsXULAtoms::sortActive, value))
    && (rv == NS_CONTENT_ATTR_HAS_VALUE))
  {
    if (value.EqualsLiteral("true"))
    {
      if (NS_SUCCEEDED(rv = tree->GetAttr(kNameSpaceID_None,
                                          nsXULAtoms::sortResource,
                                          sortResource))
          && (rv == NS_CONTENT_ATTR_HAS_VALUE))
      {
        if (NS_SUCCEEDED(rv = tree->GetAttr(kNameSpaceID_None,
                                            nsXULAtoms::sortDirection,
                                            sortDirection))
            && (rv == NS_CONTENT_ATTR_HAS_VALUE))
        {
          rv = NS_OK;

          // sort separator flag is optional
          if (NS_SUCCEEDED(rv = tree->GetAttr(kNameSpaceID_None,
                                              nsXULAtoms::sortSeparators,
                                              value)) &&
              (rv == NS_CONTENT_ATTR_HAS_VALUE))
          {
            if (value.EqualsLiteral("true"))
            {
              inbetweenSeparatorSort = PR_TRUE;
            }
          }

          // secondary sort info is optional
          if (NS_FAILED(rv = tree->GetAttr(kNameSpaceID_None,
                                           nsXULAtoms::sortResource2,
                                           sortResource2)) ||
              (rv != NS_CONTENT_ATTR_HAS_VALUE))
          {
            sortResource2.Truncate();
          }
        }
      }
    }
  }
  
  return rv;
}

nsresult
XULSortServiceImpl::CompareNodes(nsIRDFNode *cellNode1, PRBool isCollationKey1,
                                 nsIRDFNode *cellNode2, PRBool isCollationKey2,
                                 PRBool &bothValid, PRInt32 & sortOrder)
{
  bothValid = PR_FALSE;
  sortOrder = 0;

  // First, check for blobs.  This is the preferred way to do a raw key comparison.
  {
    nsCOMPtr<nsIRDFBlob> l = do_QueryInterface(cellNode1);
    if (l)
    {
      nsCOMPtr<nsIRDFBlob> r = do_QueryInterface(cellNode2);
      if (r)
      {
        const PRUint8 *lkey, *rkey;
        PRInt32 llen, rlen;
        l->GetValue(&lkey);
        l->GetLength(&llen);
        r->GetValue(&rkey);
        r->GetLength(&rlen);
        bothValid = PR_TRUE;
        if (gCollation)
          return gCollation->CompareRawSortKey(lkey, llen, rkey, rlen, &sortOrder);
      }
    }
  }

  // Next, literals.  If isCollationKey1 and 2 are both set, do
  // an unsafe raw comparison. (XXX Remove this code someday.)
  {
    nsCOMPtr<nsIRDFLiteral> l = do_QueryInterface(cellNode1);
    if (l)
    {
      nsCOMPtr<nsIRDFLiteral> r = do_QueryInterface(cellNode2);
      if (r)
      {
        const PRUnichar *luni, *runi;
        l->GetValueConst(&luni);
        r->GetValueConst(&runi);
        bothValid = PR_TRUE;
        if (isCollationKey1 && isCollationKey2)
          return gCollation->CompareRawSortKey(NS_REINTERPRET_CAST(const PRUint8*, luni),
                                               nsCRT::strlen(luni)*sizeof(PRUnichar),
                                               NS_REINTERPRET_CAST(const PRUint8*, runi),
                                               nsCRT::strlen(runi)*sizeof(PRUnichar),
                                               &sortOrder);
        else
        {
          nsresult rv = NS_ERROR_FAILURE;
          nsDependentString lstr(luni), rstr(runi);
          if (gCollation)
            rv = gCollation->CompareString(kCollationCaseInSensitive, lstr, rstr, &sortOrder);
          if (NS_FAILED(rv))
            sortOrder = Compare(lstr, rstr, nsCaseInsensitiveStringComparator());
          return NS_OK;
        }
      }
    }
  }

  // Integers.
  {
    nsCOMPtr<nsIRDFInt> l = do_QueryInterface(cellNode1);
    if (l)
    {
      nsCOMPtr<nsIRDFInt> r = do_QueryInterface(cellNode2);
      if (r)
      {
        PRInt32 lint, rint;
        l->GetValue(&lint);
        r->GetValue(&rint);
        bothValid = PR_TRUE;
        sortOrder = 0;
        if (lint < rint)
          sortOrder = -1;
        else if (lint > rint)
          sortOrder = 1;
        return NS_OK;
      }
    }
  }

  // Dates.
  {
    nsCOMPtr<nsIRDFDate> l = do_QueryInterface(cellNode1);
    if (l)
    {
      nsCOMPtr<nsIRDFDate> r = do_QueryInterface(cellNode2);
      if (r)
      {
        PRInt64 ldate, rdate, delta;
        l->GetValue(&ldate);
        r->GetValue(&rdate);
        bothValid = PR_TRUE;
        LL_SUB(delta, ldate, rdate);

        if (LL_IS_ZERO(delta))
          sortOrder = 0;
        else if (LL_GE_ZERO(delta))
          sortOrder = 1;
        else
          sortOrder = -1;
        return NS_OK;
      }
    }
  }

  // Rats.
  return NS_OK;
}

nsresult
XULSortServiceImpl::GetResourceValue(nsIRDFResource *res1, sortPtr sortInfo,
                                     PRBool first, PRBool useCache,
                                     PRBool onlyCollationHint, nsIRDFNode **target, PRBool &isCollationKey)
{
  nsresult rv = NS_OK;

  *target = nsnull;
  isCollationKey = PR_FALSE;

  if (res1 && !sortInfo->naturalOrderSort) {
    nsCOMPtr<nsIRDFResource>  modSortRes;

    // for any given property, first ask the graph for its value with "?collation=true" appended
    // to indicate that if there is a collation key available for this value, we want it

    modSortRes = (first) ? sortInfo->sortPropertyColl : sortInfo->sortPropertyColl2;
    if (modSortRes) {
      if (NS_SUCCEEDED(rv = GetCachedTarget(sortInfo, useCache, res1, modSortRes, PR_TRUE, target))
          && (rv != NS_RDF_NO_VALUE))
      {
        isCollationKey = PR_TRUE;
      }
    }
    if (!*target && !onlyCollationHint) {
      // if no collation key, ask the graph for its value with "?sort=true" appended
      // to indicate that if there is any distinction between its display value and sorting
      // value, we want the sorting value (so that, for example, a mail datasource could strip
      // off a "Re:" on a mail message subject)
      modSortRes = first ? sortInfo->sortPropertySort : sortInfo->sortPropertySort2;
      if (modSortRes)
        rv = GetCachedTarget(sortInfo, useCache, res1, modSortRes, PR_TRUE, target);
    }
    if (!*target && !onlyCollationHint) {
      // if no collation key and no special sorting value, just get the property value
      modSortRes = first ? sortInfo->sortProperty : sortInfo->sortProperty2;
      if (modSortRes)
        rv = GetCachedTarget(sortInfo, useCache, res1, modSortRes, PR_TRUE, target);
    }
  }
  
  return rv;
}

nsresult
XULSortServiceImpl::GetResourceValue(contentSortInfo *contentSortInfo, sortPtr sortInfo,
                                     PRBool first, PRBool useCache,
                                     PRBool onlyCollationHint, nsIRDFNode **target, PRBool &isCollationKey)
{
  nsresult    rv = NS_OK;

  *target = nsnull;
  isCollationKey = PR_FALSE;

  nsIRDFResource *res1 = contentSortInfo->resource;

  if (res1 && sortInfo->naturalOrderSort == PR_FALSE)
    rv = GetTarget(contentSortInfo, sortInfo, first, onlyCollationHint, PR_TRUE, target, isCollationKey);

  return rv;
}

nsresult
XULSortServiceImpl::GetCachedTarget(sortPtr sortInfo, PRBool useCache, nsIRDFResource* aSource,
    nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **aResult)
{
  nsresult  rv;
  *aResult = nsnull;

  if (!(sortInfo->mInner)) {
    // if we don't have a mInner, create one
    sortInfo->mInner = do_CreateInstance(kRDFInMemoryDataSourceCID, &rv);
    if (NS_FAILED(rv)) return  rv;
  }

  rv = NS_RDF_NO_VALUE;
  if (sortInfo->mInner) {
    if (useCache) {
      // if we do have a mInner, look for the resource in it
      rv = sortInfo->mInner->GetTarget(aSource, aProperty, aTruthValue, aResult);
    } else if (sortInfo->db) {
      // if we don't have a cached value, look it up in the document's DB
      if (NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(aSource, aProperty, aTruthValue, aResult))
          && (rv != NS_RDF_NO_VALUE))
      {
        // and if we have a value, cache it away in our mInner also (ignore errors)
        sortInfo->mInner->Assert(aSource, aProperty, *aResult, PR_TRUE);
      }
    }
  }
  
  return rv;
}

nsresult 
XULSortServiceImpl::GetTarget(contentSortInfo *contentSortInfo, sortPtr sortInfo,
                              PRBool first, PRBool onlyCollationHint, PRBool truthValue,
                              nsIRDFNode **target, PRBool &isCollationKey)
{
  nsresult rv;
  nsIRDFResource *resource = contentSortInfo->resource;

  if (first) {
    if (contentSortInfo->collationNode1) {
      *target = contentSortInfo->collationNode1;
      NS_IF_ADDREF(*target);
    } else if (!contentSortInfo->checkedCollation1
               && NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(resource, sortInfo->sortPropertyColl, truthValue, target)))
    {
      if (rv != NS_RDF_NO_VALUE)
        contentSortInfo->collationNode1 = *target;
      contentSortInfo->checkedCollation1 = PR_TRUE;
    }
  
    if (*target) {
      isCollationKey = PR_TRUE;
      return NS_OK;
    }

    if (onlyCollationHint == PR_FALSE)
    {
      if (contentSortInfo->sortNode1) {
        *target = contentSortInfo->sortNode1;
        NS_IF_ADDREF(*target);
      } else if (!contentSortInfo->checkedSort1 
                 && NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(resource, sortInfo->sortPropertySort, truthValue, target)))
      {
        if (rv != NS_RDF_NO_VALUE)
          contentSortInfo->sortNode1 = *target;
        contentSortInfo->checkedSort1 = PR_TRUE;
      }

      if (*target)
        return NS_OK;

      if (contentSortInfo->node1) {
        *target = contentSortInfo->node1;
        NS_IF_ADDREF(*target);
      } else if (!contentSortInfo->checkedNode1 
                 && NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(resource, sortInfo->sortProperty, truthValue, target)))
      {
        if (rv != NS_RDF_NO_VALUE)
          contentSortInfo->node1 = *target;
        contentSortInfo->checkedNode1 = PR_TRUE;
      }

      if (*target)
        return NS_OK;
    }
  } else {
    if (contentSortInfo->collationNode2) {
      *target = contentSortInfo->collationNode2;
      NS_IF_ADDREF(*target);
    } else if (!contentSortInfo->checkedCollation2
               && NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(resource, sortInfo->sortPropertyColl2, truthValue, target)))
    {
      if (rv != NS_RDF_NO_VALUE)
        contentSortInfo->collationNode2 = *target;
      contentSortInfo->checkedCollation2 = PR_TRUE;
    }
  
    if (*target) {
      isCollationKey = PR_TRUE;
      return NS_OK;
    }

    if (onlyCollationHint == PR_FALSE) {
      if (contentSortInfo->sortNode2) {
        *target = contentSortInfo->sortNode2;
        NS_IF_ADDREF(*target);
      } else if (!contentSortInfo->checkedSort2
                 && NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(resource, sortInfo->sortPropertySort2, truthValue, target)))
      {
        if (rv != NS_RDF_NO_VALUE)
          contentSortInfo->sortNode2 = *target;
        contentSortInfo->checkedSort2 = PR_TRUE;
      }

      if (*target)
        return NS_OK;

      if (contentSortInfo->node2) {
        *target = contentSortInfo->node2;
        NS_IF_ADDREF(*target);
      } else if (!contentSortInfo->checkedNode2
                 && NS_SUCCEEDED(rv = (sortInfo->db)->GetTarget(resource, sortInfo->sortProperty2, truthValue, target)))
      {
        if (rv != NS_RDF_NO_VALUE)
          contentSortInfo->node2 = *target;
        contentSortInfo->checkedNode2 = PR_TRUE;
      }

      if (*target)
        return NS_OK;
    }
  }
      
  return NS_RDF_NO_VALUE;
}

nsresult
XULSortServiceImpl::GetNodeValue(nsIContent *node1, sortPtr sortInfo, PRBool first,
        PRBool onlyCollationHint, nsIRDFNode **theNode, PRBool &isCollationKey)
{
  nsresult rv;
  nsCOMPtr<nsIRDFResource>  res1;

  *theNode = nsnull;
  isCollationKey = PR_FALSE;

  nsCOMPtr<nsIDOMXULElement>  dom1 = do_QueryInterface(node1);
  if (dom1) {
    if (NS_FAILED(rv = dom1->GetResource(getter_AddRefs(res1))))
      res1 = nsnull;
    // Note: don't check for res1 QI failure here.  It only succeeds for RDF nodes,
    // but for XUL nodes it will failure; in the failure case, the code below gets
    // the cell's text value straight from the DOM
  } else {
    nsCOMPtr<nsIDOMElement>  htmlDom = do_QueryInterface(node1);
    if (htmlDom) {
      nsAutoString htmlID;
      if (NS_SUCCEEDED(rv = node1->GetAttr(kNameSpaceID_None, nsXULAtoms::id, htmlID)) 
          && (rv == NS_CONTENT_ATTR_HAS_VALUE))
      {
        if (NS_FAILED(rv = gRDFService->GetUnicodeResource(htmlID, getter_AddRefs(res1))))
          res1 = nsnull;
      }
    }
    else
    {
      return NS_ERROR_FAILURE;
    }
  }
  
  if ((sortInfo->naturalOrderSort == PR_FALSE) && (sortInfo->sortProperty)) {
    if (res1) {
      rv = GetResourceValue(res1, sortInfo, first, PR_TRUE, onlyCollationHint, theNode, isCollationKey);
      if ((rv == NS_RDF_NO_VALUE) || (!*theNode))
        rv = GetResourceValue(res1, sortInfo, first, PR_FALSE, onlyCollationHint, theNode, isCollationKey);
    }
    else
      rv = NS_RDF_NO_VALUE;

  } else if ((sortInfo->naturalOrderSort == PR_TRUE) && (sortInfo->parentContainer)) {
    nsAutoString    cellPosVal1;

    // check to see if this is a RDF_Seq
    // Note: this code doesn't handle the aggregated Seq case especially well
    if ((res1) && (sortInfo->db))
    {
      nsCOMPtr<nsIRDFResource>  parentResource;
      nsCOMPtr<nsIDOMXULElement>  parentDOMNode = do_QueryInterface(sortInfo->parentContainer);
      if (parentDOMNode)
      {
        if (NS_FAILED(rv = parentDOMNode->GetResource(getter_AddRefs(parentResource))))
        {
          parentResource = nsnull;
        }
      }

      if (parentResource)
      {
        PRInt32 index;
        rv = gRDFC->IndexOf(sortInfo->db, parentResource,
                res1, &index);
        if (index != -1)
        {
          nsCOMPtr<nsIRDFInt> intLit;
          rv = gRDFService->GetIntLiteral(index, getter_AddRefs(intLit));
          CallQueryInterface(intLit, theNode);
          isCollationKey = PR_FALSE;
        }
      }
    }
  }
  return rv;
}

nsresult
XULSortServiceImpl::GetNodeValue(contentSortInfo *info1, sortPtr sortInfo, PRBool first,
                                 PRBool onlyCollationHint, nsIRDFNode **theNode,
                                 PRBool &isCollationKey)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource>  res1;

  nsIContent *node1 = info1->content;
  *theNode = nsnull;
  isCollationKey = PR_FALSE;

  // determine the rdf resource
  nsCOMPtr<nsIDOMXULElement>  dom1 = do_QueryInterface(node1);
  if (dom1)
    res1 = info1->resource;
  else {
    // If this isn't a XUL element, get its id and fetch the resource directly
    nsCOMPtr<nsIDOMElement>  htmlDom = do_QueryInterface(node1);
    if (htmlDom) {
      nsAutoString htmlID;
      if (NS_SUCCEEDED(rv = node1->GetAttr(kNameSpaceID_None, nsXULAtoms::id, htmlID))
        && (rv == NS_CONTENT_ATTR_HAS_VALUE))
      {
        if (NS_FAILED(rv = gRDFService->GetUnicodeResource(htmlID, getter_AddRefs(res1))))
          res1 = nsnull;
        info1->resource = res1;
      }
    }
    else
      return NS_ERROR_FAILURE;
  }
  
  if ((!sortInfo->naturalOrderSort) && (sortInfo->sortProperty)) {
    if (res1) {
      rv = GetResourceValue(info1, sortInfo, first, PR_TRUE, onlyCollationHint, theNode, isCollationKey);
      if ((rv == NS_RDF_NO_VALUE) || (!*theNode)) {
        rv = GetResourceValue(info1, sortInfo, first, PR_FALSE, onlyCollationHint, theNode, isCollationKey);
      }
    }
    else
      rv = NS_RDF_NO_VALUE;

  } else if ((sortInfo->naturalOrderSort == PR_TRUE) && (sortInfo->parentContainer)) {
    nsAutoString  cellPosVal1;

    // check to see if this is a RDF_Seq
    // Note: this code doesn't handle the aggregated Seq case especially well
    if ((res1) && (sortInfo->db))
    {
      nsCOMPtr<nsIRDFResource>  parentResource;
      nsCOMPtr<nsIDOMXULElement>  parentDOMNode = do_QueryInterface(sortInfo->parentContainer);
      if (parentDOMNode)
      {
        if (NS_FAILED(rv = parentDOMNode->GetResource(getter_AddRefs(parentResource))))
        {
          parentResource = nsnull;
        }
      }

      if (parentResource)
      {
        PRInt32 index;
        rv = gRDFC->IndexOf(sortInfo->db, parentResource,
                res1, &index);
        if (index != -1)
        {
          nsCOMPtr<nsIRDFInt> intLit;
          rv = gRDFService->GetIntLiteral(index, getter_AddRefs(intLit));
          CallQueryInterface(intLit, theNode);
          isCollationKey = PR_FALSE;
        }
      }
    }
  }
  else
    // XXX Is this right?
    rv = NS_ERROR_NULL_POINTER;

  return rv;
}

nsresult
XULSortServiceImpl::InplaceSort(nsIContent *node1, nsIContent *node2, sortPtr sortInfo, PRInt32 & sortOrder)
{
  PRBool isCollationKey1 = PR_FALSE, isCollationKey2 = PR_FALSE;
  nsresult rv;

  sortOrder = 0;

  nsCOMPtr<nsIRDFNode> cellNode1, cellNode2;

  // rjc: in some cases, the first node is static while the second node changes
  // per comparison; in these circumstances, we can cache the first node
  if ((sortInfo->cacheFirstHint == PR_TRUE) && (sortInfo->cacheFirstNode))
  {
    cellNode1 = sortInfo->cacheFirstNode;
    isCollationKey1 = sortInfo->cacheIsFirstNodeCollationKey;
  }
  else
  {
    GetNodeValue(node1, sortInfo, PR_TRUE, PR_FALSE, getter_AddRefs(cellNode1), isCollationKey1);
    if (sortInfo->cacheFirstHint == PR_TRUE)
    {
      sortInfo->cacheFirstNode = cellNode1;
      sortInfo->cacheIsFirstNodeCollationKey = isCollationKey1;
    }
  }
  GetNodeValue(node2, sortInfo, PR_TRUE, isCollationKey1, getter_AddRefs(cellNode2), isCollationKey2);

  PRBool  bothValid = PR_FALSE;
  rv = CompareNodes(cellNode1, isCollationKey1, cellNode2, isCollationKey2, bothValid, sortOrder);

  if (sortOrder == 0)
  {
    // nodes appear to be equivalent, check for secondary sort criteria
    if (sortInfo->sortProperty2 != nsnull)
    {
      cellNode1 = nsnull;
      cellNode2 = nsnull;
      isCollationKey1 = PR_FALSE;
      isCollationKey2 = PR_FALSE;
      
      GetNodeValue(node1, sortInfo, PR_FALSE, PR_FALSE, getter_AddRefs(cellNode1), isCollationKey1);
      GetNodeValue(node2, sortInfo, PR_FALSE, isCollationKey1, getter_AddRefs(cellNode2), isCollationKey2);

      bothValid = PR_FALSE;
      rv = CompareNodes(cellNode1, isCollationKey1, cellNode2, isCollationKey2, bothValid, sortOrder);
    }
  }

  if ((bothValid == PR_TRUE) && (sortInfo->descendingSort == PR_TRUE))
  {
    // descending sort is being imposed, so reverse the sort order
    sortOrder = -sortOrder;
  }

  return NS_OK;
}

nsresult
XULSortServiceImpl::InplaceSort(contentSortInfo *info1, contentSortInfo *info2, sortPtr sortInfo, PRInt32 & sortOrder)
{
  PRBool isCollationKey1 = PR_FALSE, isCollationKey2 = PR_FALSE;
  nsresult rv;

  sortOrder = 0;

  nsCOMPtr<nsIRDFNode> cellNode1, cellNode2;

  // rjc: in some cases, the first node is static while the second node changes
  // per comparison; in these circumstances, we can cache the first node
  if ((sortInfo->cacheFirstHint == PR_TRUE) && (sortInfo->cacheFirstNode))
  {
    cellNode1 = sortInfo->cacheFirstNode;
    isCollationKey1 = sortInfo->cacheIsFirstNodeCollationKey;
  }
  else
  {
    GetNodeValue(info1, sortInfo, PR_TRUE, PR_FALSE, getter_AddRefs(cellNode1), isCollationKey1);
    if (sortInfo->cacheFirstHint == PR_TRUE)
    {
      sortInfo->cacheFirstNode = cellNode1;
      sortInfo->cacheIsFirstNodeCollationKey = isCollationKey1;
    }
  }
  GetNodeValue(info2, sortInfo, PR_TRUE, isCollationKey1, getter_AddRefs(cellNode2), isCollationKey2);

  PRBool  bothValid = PR_FALSE;
  rv = CompareNodes(cellNode1, isCollationKey1, cellNode2, isCollationKey2, bothValid, sortOrder);

  if (sortOrder == 0) {
    // nodes appear to be equivalent, check for secondary sort criteria
    if (sortInfo->sortProperty2 != nsnull) {
      cellNode1 = nsnull;
      cellNode2 = nsnull;
      isCollationKey1 = PR_FALSE;
      isCollationKey2 = PR_FALSE;
    
      GetNodeValue(info1, sortInfo, PR_FALSE, PR_FALSE, getter_AddRefs(cellNode1), isCollationKey1);
      GetNodeValue(info2, sortInfo, PR_FALSE, isCollationKey1, getter_AddRefs(cellNode2), isCollationKey2);

      bothValid = PR_FALSE;
      rv = CompareNodes(cellNode1, isCollationKey1, cellNode2, isCollationKey2, bothValid, sortOrder);
    }
  }

  if ((bothValid == PR_TRUE) && (sortInfo->descendingSort == PR_TRUE))
  {
    // descending sort is being imposed, so reverse the sort order
    sortOrder = -sortOrder;
  }

  return NS_OK;
}

int PR_CALLBACK
inplaceSortCallback(const void *data1, const void *data2, void *privateData)
{
  /// Note: inplaceSortCallback is a small C callback stub for NS_QuickSort
  _sortStruct *sortInfo = (_sortStruct *)privateData;
  nsIContent *node1 = *(nsIContent **)data1;
  nsIContent *node2 = *(nsIContent **)data2;
  PRInt32 sortOrder = 0;

  if (nsnull != sortInfo)
    XULSortServiceImpl::InplaceSort(node1, node2, sortInfo, sortOrder);

  return sortOrder;
}

int PR_CALLBACK
testSortCallback(const void *data1, const void *data2, void *privateData)
{
  /// Note: testSortCallback is a small C callback stub for NS_QuickSort
  _sortStruct *sortInfo = (_sortStruct *)privateData;
  contentSortInfo *info1 = *(contentSortInfo **)data1;
  contentSortInfo *info2 = *(contentSortInfo **)data2;
  PRInt32 sortOrder = 0;

  if (nsnull != sortInfo)
    XULSortServiceImpl::InplaceSort(info1, info2, sortInfo, sortOrder);

  return sortOrder;
}

static contentSortInfo *
CreateContentSortInfo(nsIContent *content, nsIRDFResource * resource)
{
  contentSortInfo * info = new contentSortInfo;
  if (!info)
    return nsnull;

  info->content = content;
  NS_IF_ADDREF(info->content);

  info->resource = resource;

  info->checkedCollation1 = PR_FALSE;
  info->checkedCollation2 = PR_FALSE;
  info->checkedSort1 = PR_FALSE;
  info->checkedSort2 = PR_FALSE;
  info->checkedNode1 = PR_FALSE;
  info->checkedNode2 = PR_FALSE;

  return info;
}

nsresult
XULSortServiceImpl::SortContainer(nsIContent *container, sortPtr sortInfo,
                                  PRBool merelyInvertFlag)
{
  PRUint32 loop, numElements = 0, currentElement;

  PRUint32 numChildren = container->GetChildCount();
  if (numChildren < 1)
    return NS_OK;

  nsIDocument *doc = container->GetDocument();
  if (!doc)
    return NS_ERROR_UNEXPECTED;

  // Note: This is a straight allocation (not a COMPtr) so we
  // can't return out of this routine until/unless we free it!
  contentSortInfo ** contentSortInfoArray = new contentSortInfo*[numChildren + 1];
  if (!contentSortInfoArray)
    return NS_ERROR_OUT_OF_MEMORY;

  // Note: walk backwards (and add nodes into the array backwards) because
  // we also remove the nodes in this loop [via RemoveChildAt()] and if we
  // were to do this in a forward-looking manner it would be harder
  // (since we also skip over non XUL:treeitem nodes)

  nsresult rv;
  currentElement = numChildren;
  PRUint32 childIndex;
  // childIndex is unsigned, so childIndex >= 0 would always test true
  for (childIndex = numChildren; childIndex > 0; ) {
    --childIndex;
    nsIContent *child = container->GetChildAt(childIndex);

    if (child->IsContentOfType(nsIContent::eXUL)) {
      nsIAtom *tag = child->Tag();

      if (tag == nsXULAtoms::listitem ||
          tag == nsXULAtoms::treeitem ||
          tag == nsXULAtoms::menu ||
          tag == nsXULAtoms::menuitem) {
        --currentElement;

        nsCOMPtr<nsIRDFResource>  resource;
        nsXULContentUtils::GetElementResource(child, getter_AddRefs(resource));
        contentSortInfo *contentInfo = CreateContentSortInfo(child, resource);
        if (contentInfo)
          contentSortInfoArray[currentElement] = contentInfo;

        ++numElements;
      }
    }
  }
  
  if (numElements > 0) {
    /* smart sorting (sort within separators) on name column */
    if (sortInfo->inbetweenSeparatorSort) {
      PRUint32 startIndex = currentElement;
      nsAutoString  type;
      for (loop = currentElement; loop < currentElement + numElements;
           ++loop) {
        if (NS_SUCCEEDED(rv = contentSortInfoArray[loop]->content->GetAttr(kNameSpaceID_RDF,
            nsXULAtoms::type, type)) && (rv == NS_CONTENT_ATTR_HAS_VALUE))
        {
          if (type.EqualsASCII(kURINC_BookmarkSeparator)) {
            if (loop > startIndex + 1) {
              if (merelyInvertFlag)
                InvertSortInfo(&contentSortInfoArray[startIndex], loop-startIndex);
              else
                NS_QuickSort((void*)&contentSortInfoArray[startIndex], loop-startIndex,
                             sizeof(contentSortInfo*), testSortCallback, (void*)sortInfo);
              startIndex = loop+1;
            }
          }
        }
      }
      
      if (loop > startIndex+1) {
        if (merelyInvertFlag)
          InvertSortInfo(&contentSortInfoArray[startIndex], loop-startIndex);
        else
          NS_QuickSort((void*)&contentSortInfoArray[startIndex], loop-startIndex,
                       sizeof(contentSortInfo*), testSortCallback, (void*)sortInfo);
        startIndex = loop+1;
      }
    } else {
      if (merelyInvertFlag)
        InvertSortInfo(&contentSortInfoArray[currentElement], numElements);
      else
        NS_QuickSort((void*)(&contentSortInfoArray[currentElement]), numElements,
                     sizeof(contentSortInfo*), testSortCallback, (void*)sortInfo);
    }

    // childIndex is unsigned, so childIndex >= 0 would always test true
    for (childIndex = numChildren; childIndex > 0; )
    {
      --childIndex;
      nsIContent *child = container->GetChildAt(childIndex);

      if (child->IsContentOfType(nsIContent::eXUL)) {
        nsIAtom *tag = child->Tag();

        if (tag == nsXULAtoms::listitem ||
            tag == nsXULAtoms::treeitem ||
            tag == nsXULAtoms::menu ||
            tag == nsXULAtoms::menuitem) {
          // immediately remove the child node, and ignore any errors
          container->RemoveChildAt(childIndex, PR_FALSE);
        }
      }
    }

    // put the items back in sorted order
    nsAutoString value;
    PRUint32 childPos = container->GetChildCount();
    for (loop = currentElement; loop < currentElement + numElements; loop++) {
      contentSortInfo * contentSortInfo = contentSortInfoArray[loop];
      nsIContent *parentNode = (nsIContent *)contentSortInfo->content;
      nsIContent* kid = parentNode;
      container->InsertChildAt(kid, childPos++, PR_FALSE, PR_TRUE);

      NS_RELEASE(contentSortInfo->content);
      delete contentSortInfo;

      // if it's a container, find its treechildren nodes, and sort those
      if (NS_FAILED(rv = parentNode->GetAttr(kNameSpaceID_None, nsXULAtoms::container, value)) ||
        (rv != NS_CONTENT_ATTR_HAS_VALUE) || !value.EqualsLiteral("true"))
        continue;
        
      numChildren = parentNode->GetChildCount();

      for (childIndex = 0; childIndex < numChildren; childIndex++) {
        nsIContent *child = parentNode->GetChildAt(childIndex);

        nsINodeInfo *ni = child->GetNodeInfo();

        if (ni && (ni->Equals(nsXULAtoms::treechildren, kNameSpaceID_XUL) ||
                   ni->Equals(nsXULAtoms::menupopup, kNameSpaceID_XUL))) {
          sortInfo->parentContainer = parentNode;
          SortContainer(child, sortInfo, merelyInvertFlag);
        }
      }
    }
  }
  
  delete [] contentSortInfoArray;
  contentSortInfoArray = nsnull;

  return NS_OK;
}

nsresult
XULSortServiceImpl::InvertSortInfo(contentSortInfo **data, PRInt32 numItems)
{
  if (numItems > 1) {
    PRInt32 upPoint = (numItems + 1)/2, downPoint = (numItems - 2)/2;
    PRInt32 half = numItems/2;
    while (half-- > 0) {
      contentSortInfo *temp = data[downPoint];
      data[downPoint--] = data[upPoint];
      data[upPoint++] = temp;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
XULSortServiceImpl::InsertContainerNode(nsIRDFCompositeDataSource *db, nsRDFSortState *sortState,
                                        nsIContent *root, nsIContent *trueParent, nsIContent *container,
                                        nsIContent *node, PRBool aNotify)
{
  nsresult rv;
  nsAutoString sortResource, sortDirection, sortResource2;
  _sortStruct sortInfo;

  // get composite db for tree
  sortInfo.db = db;
  sortInfo.parentContainer = trueParent;
  sortInfo.sortProperty = nsnull;
  sortInfo.sortProperty2 = nsnull;
  sortInfo.inbetweenSeparatorSort = PR_FALSE;
  sortInfo.cacheFirstHint = PR_TRUE;

  if (sortState->mCache)
	sortInfo.mInner = sortState->mCache;
  else
	sortInfo.mInner = nsnull;

  if (container != sortState->lastContainer.get()) {
    sortState->lastContainer = container;
    sortState->lastWasFirst = PR_FALSE;
    sortState->lastWasLast = PR_FALSE;
  }

  PRBool sortInfoAvailable = PR_FALSE;

  // look for sorting info on root node

  if (NS_SUCCEEDED(rv = root->GetAttr(kNameSpaceID_None, nsXULAtoms::sortResource, sortResource))
      && (rv == NS_CONTENT_ATTR_HAS_VALUE))
  {
    if (NS_SUCCEEDED(rv = root->GetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, sortDirection))
        && (rv == NS_CONTENT_ATTR_HAS_VALUE))
    {
      sortInfoAvailable = PR_TRUE;

      if (NS_FAILED(rv = root->GetAttr(kNameSpaceID_None, nsXULAtoms::sortResource2, sortResource2)) 
          || (rv != NS_CONTENT_ATTR_HAS_VALUE))
      {
        sortResource2.Truncate();
      }
    }
  }

  if (sortInfoAvailable) {
    if (sortState->sortResource.Equals(sortResource) 
        && sortState->sortResource2.Equals(sortResource2))
    {
      sortInfo.sortProperty = sortState->sortProperty;
      sortInfo.sortProperty2 = sortState->sortProperty2;
      sortInfo.sortPropertyColl = sortState->sortPropertyColl;
      sortInfo.sortPropertyColl2 = sortState->sortPropertyColl2;
      sortInfo.sortPropertySort = sortState->sortPropertySort;
      sortInfo.sortPropertySort2 = sortState->sortPropertySort2;
    }
    else {
      // either first time, or must have changing sorting info, so flush state cache
      sortState->sortProperty = nsnull;
      sortState->sortProperty2 = nsnull;
      sortState->sortPropertyColl = nsnull;
      sortState->sortPropertyColl2 = nsnull;
      sortState->sortPropertySort = nsnull;
      sortState->sortPropertySort2 = nsnull;

      rv = gRDFService->GetUnicodeResource(sortResource, getter_AddRefs(sortInfo.sortProperty));
      if (NS_FAILED(rv)) return rv;
      sortState->sortResource = sortResource;
      sortState->sortProperty = sortInfo.sortProperty;
      
      nsAutoString resourceUrl = sortResource;
      resourceUrl.AppendLiteral("?collation=true");
      rv = gRDFService->GetUnicodeResource(resourceUrl, getter_AddRefs(sortInfo.sortPropertyColl));
      if (NS_FAILED(rv)) return rv;
      sortState->sortPropertyColl = sortInfo.sortPropertyColl;

      resourceUrl = sortResource;
      resourceUrl.AppendLiteral("?sort=true");
      rv = gRDFService->GetUnicodeResource(resourceUrl, getter_AddRefs(sortInfo.sortPropertySort));
      if (NS_FAILED(rv)) return rv;
      sortState->sortPropertySort = sortInfo.sortPropertySort;

      if (!sortResource2.IsEmpty()) {
        rv = gRDFService->GetUnicodeResource(sortResource2, getter_AddRefs(sortInfo.sortProperty2));
        if (NS_FAILED(rv)) return rv;
        sortState->sortResource2 = sortResource2;
        sortState->sortProperty2 = sortInfo.sortProperty2;

        resourceUrl = sortResource2;
        resourceUrl.AppendLiteral("?collation=true");
        rv = gRDFService->GetUnicodeResource(resourceUrl, getter_AddRefs(sortInfo.sortPropertyColl2));
        if (NS_FAILED(rv)) return rv;
        sortState->sortPropertyColl2 = sortInfo.sortPropertyColl2;

        resourceUrl = sortResource2;
        resourceUrl.AppendLiteral("?sort=true");
        rv = gRDFService->GetUnicodeResource(resourceUrl, getter_AddRefs(sortInfo.sortPropertySort2));
        if (NS_FAILED(rv)) return rv;
        sortState->sortPropertySort2 = sortInfo.sortPropertySort2;
      }
    }
  } else {
    // either first time, or must have changing sorting info, so flush state cache
    sortState->sortResource.Truncate();
    sortState->sortResource2.Truncate();

    sortState->sortProperty = nsnull;
    sortState->sortProperty2 = nsnull;
    sortState->sortPropertyColl = nsnull;
    sortState->sortPropertyColl2 = nsnull;
    sortState->sortPropertySort = nsnull;
    sortState->sortPropertySort2 = nsnull;
  }

  // set up sort order info
  sortInfo.naturalOrderSort = PR_FALSE;
  sortInfo.descendingSort = PR_FALSE;
  if (sortDirection.Equals(*kDescendingStr))
    sortInfo.descendingSort = PR_TRUE;
  else if (!sortDirection.Equals(*kAscendingStr))
    sortInfo.naturalOrderSort = PR_TRUE;

  PRBool isContainerRDFSeq = PR_FALSE;
  
  if (sortInfo.db && sortInfo.naturalOrderSort) {
    // walk up the content model to find the REAL
    // parent container to determine if its a RDF_Seq
    nsCOMPtr<nsIContent> parent = do_QueryInterface(container, &rv);
    nsCOMPtr<nsIContent> aContent;

    nsCOMPtr<nsIDocument> doc;
    if (NS_SUCCEEDED(rv) && parent) {
      doc = parent->GetDocument();
      if (!doc)
        parent = nsnull;
    }

    if (parent) {
      nsAutoString id;

      rv = trueParent->GetAttr(kNameSpaceID_None, nsXULAtoms::ref, id);
      if (id.IsEmpty())
        rv = trueParent->GetAttr(kNameSpaceID_None, nsXULAtoms::id, id);

      if (!id.IsEmpty()) {
        nsCOMPtr<nsIRDFResource> containerRes;
        rv = nsXULContentUtils::MakeElementResource(doc, id, getter_AddRefs(containerRes));
        if (NS_SUCCEEDED(rv))
          rv = gRDFC->IsSeq(sortInfo.db, containerRes,  &isContainerRDFSeq);
      }
    }
  }

  PRBool childAdded = PR_FALSE;
  PRUint32 numChildren = container->GetChildCount();

  if ((sortInfo.naturalOrderSort == PR_FALSE) ||
      ((sortInfo.naturalOrderSort == PR_TRUE) &&
       (isContainerRDFSeq == PR_TRUE)))
  {
    // because numChildren gets modified
    PRInt32 realNumChildren = numChildren;
    nsIContent *child = nsnull;

    // rjc says: determine where static XUL ends and generated XUL/RDF begins
    PRInt32 staticCount = 0;

    nsAutoString staticValue;
    if (NS_SUCCEEDED(rv = container->GetAttr(kNameSpaceID_None, nsXULAtoms::staticHint, staticValue))
        && (rv == NS_CONTENT_ATTR_HAS_VALUE))
    {
      // found "static" XUL element count hint
      PRInt32 strErr=0;
      staticCount = staticValue.ToInteger(&strErr);
      if (strErr)
        staticCount = 0;
    } else {
      // compute the "static" XUL element count
      nsAutoString  valueStr;
      for (PRUint32 childLoop = 0; childLoop < numChildren; ++childLoop) {
        child = container->GetChildAt(childLoop);
        if (!child) break;

        if (NS_SUCCEEDED(rv = child->GetAttr(kNameSpaceID_None, nsXULAtoms::templateAtom, valueStr))
            && (rv == NS_CONTENT_ATTR_HAS_VALUE))
          break;
        else
          ++staticCount;
      }
      
      if (NS_SUCCEEDED(rv = root->GetAttr(kNameSpaceID_None, nsXULAtoms::sortStaticsLast, valueStr))
          && (rv == NS_CONTENT_ATTR_HAS_VALUE) && valueStr.EqualsLiteral("true"))
      {
        // indicate that static XUL comes after RDF-generated content by making negative
        staticCount = -staticCount;
      }

      // save the "static" XUL element count hint
      valueStr.Truncate();
      valueStr.AppendInt(staticCount);
      container->SetAttr(kNameSpaceID_None, nsXULAtoms::staticHint, valueStr, PR_FALSE);
    }

    if (staticCount <= 0) {
      numChildren += staticCount;
      staticCount = 0;
    } else if (staticCount > (PRInt32)numChildren) {
      staticCount = numChildren;
      numChildren -= staticCount;
    }

    // figure out where to insert the node when a sort order is being imposed
    if (numChildren > 0) {
      nsIContent *temp;
      PRInt32 direction;

      // rjc says: The following is an implementation of a fairly optimal
      // binary search insertion sort... with interpolation at either end-point.

      if (sortState->lastWasFirst) {
        child = container->GetChildAt(staticCount);
        temp = child;
        direction = inplaceSortCallback(&node, &temp, &sortInfo);
        if (direction < 0) {
          container->InsertChildAt(node, staticCount, aNotify, PR_FALSE);
          childAdded = PR_TRUE;
        } else
          sortState->lastWasFirst = PR_FALSE;
      } else if (sortState->lastWasLast) {
        child = container->GetChildAt(realNumChildren - 1);
        temp = child;
        direction = inplaceSortCallback(&node, &temp, &sortInfo);
        if (direction > 0) {
          container->InsertChildAt(node, realNumChildren, aNotify, PR_FALSE);
          childAdded = PR_TRUE;
        } else
          sortState->lastWasLast = PR_FALSE;
      }

      PRInt32 left = staticCount+1, right = realNumChildren, x;
      while (!childAdded && right >= left) {
        x = (left + right) / 2;
        child = container->GetChildAt(x - 1);
        temp = child;

        // rjc says: since cacheFirstHint is PR_TRUE, the first node passed
        // into inplaceSortCallback() must be the node that doesn't change

        direction = inplaceSortCallback(&node, &temp, &sortInfo);
        if (((x == left) && (direction < 0)) || (((x == right))
             && (direction >= 0)) || (left == right))
        {
          PRInt32 thePos = ((direction > 0) ? x : x-1);
          container->InsertChildAt(node, thePos, aNotify, PR_FALSE);
          childAdded = PR_TRUE;
          
          sortState->lastWasFirst = (thePos == staticCount) ? PR_TRUE: PR_FALSE;
          sortState->lastWasLast = (thePos >= realNumChildren) ? PR_TRUE: PR_FALSE;
          
          break;
        }
        if (direction < 0)
          right = x-1;
        else
          left = x+1;
      }
    }
  }

  if (!childAdded)
    container->InsertChildAt(node, numChildren, aNotify, PR_FALSE);

  if (!sortState->mCache && sortInfo.mInner)
    sortState->mCache = sortInfo.mInner;

  return NS_OK;
}

NS_IMETHODIMP
XULSortServiceImpl::Sort(nsIDOMNode* node, const nsAString& sortResource, const nsAString& sortDirection)
{
  nsresult rv;
  _sortStruct  sortInfo;

  // get root content node
  nsCOMPtr<nsIContent>  contentNode = do_QueryInterface(node);
  if (!contentNode) return NS_ERROR_FAILURE;
  nsCOMPtr<nsIContent> dbNode;
  if (NS_FAILED(rv = FindDatabaseElement(contentNode, getter_AddRefs(dbNode))))
    return rv;
  nsCOMPtr<nsIDOMXULElement> dbXULNode = do_QueryInterface(dbNode);
  if (!dbXULNode) return NS_ERROR_FAILURE;

  // get composite db
  sortInfo.db = nsnull;
  sortInfo.mInner = nsnull;
  sortInfo.parentContainer = dbNode;
  sortInfo.inbetweenSeparatorSort = PR_FALSE;
  sortInfo.cacheFirstHint = PR_FALSE;

  // optimization - if we're about to merely invert the current sort
  // then just reverse-index the current tree
  PRBool invertTreeFlag = PR_FALSE;
  nsAutoString value;
  if (NS_SUCCEEDED(rv = dbNode->GetAttr(kNameSpaceID_None, nsXULAtoms::sortActive, value))
      && (rv == NS_CONTENT_ATTR_HAS_VALUE) 
      && value.EqualsLiteral("true"))
  {
    if (NS_SUCCEEDED(rv = dbNode->GetAttr(kNameSpaceID_None, nsXULAtoms::sortResource, value))
        && (rv == NS_CONTENT_ATTR_HAS_VALUE) 
        && (value.Equals(sortResource)))
    {
      if (NS_SUCCEEDED(rv = dbNode->GetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, value))
          && (rv == NS_CONTENT_ATTR_HAS_VALUE))
      {
        if ((value.Equals(*kDescendingStr) && sortDirection.Equals(*kAscendingStr)) 
            || (value.Equals(*kAscendingStr) && sortDirection.Equals(*kDescendingStr))) 
        {
          invertTreeFlag = PR_TRUE;
        }
      }
    }
  }

  // remove any sort hints on tree root node
  dbNode->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortActive, PR_FALSE);
  dbNode->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, PR_FALSE);
  dbNode->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortSeparators, PR_FALSE);
  dbNode->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortResource, PR_FALSE);
  dbNode->UnsetAttr(kNameSpaceID_None, nsXULAtoms::sortResource2, PR_FALSE);

  nsCOMPtr<nsIRDFCompositeDataSource>  cds;
  if (NS_SUCCEEDED(rv = dbXULNode->GetDatabase(getter_AddRefs(cds))))
    sortInfo.db = cds;

  // determine new sort resource and direction to use
  if (sortDirection.Equals(*kNaturalStr)) {
    sortInfo.naturalOrderSort = PR_TRUE;
    sortInfo.descendingSort = PR_FALSE;
  } else {
    sortInfo.naturalOrderSort = PR_FALSE;
    if (sortDirection.Equals(*kAscendingStr))
      sortInfo.descendingSort = PR_FALSE;
    else if (sortDirection.Equals(*kDescendingStr))
      sortInfo.descendingSort = PR_TRUE;
  }
  
  // look for additional sort info from content
  nsAutoString sortResource2, unused;
  GetSortColumnInfo(contentNode, unused, unused, sortResource2, sortInfo.inbetweenSeparatorSort);

  // build resource url for first sort resource
  rv = gRDFService->GetUnicodeResource(sortResource, getter_AddRefs(sortInfo.sortProperty));
  if (NS_FAILED(rv)) return rv;

  nsAutoString resourceUrl;
  resourceUrl.Assign(sortResource);
  resourceUrl.AppendLiteral("?collation=true");
  rv = gRDFService->GetUnicodeResource(resourceUrl, getter_AddRefs(sortInfo.sortPropertyColl));
  if (NS_FAILED(rv)) return rv;

  resourceUrl.Assign(sortResource);
  resourceUrl.AppendLiteral("?sort=true");
  rv = gRDFService->GetUnicodeResource(resourceUrl, getter_AddRefs(sortInfo.sortPropertySort));
  if (NS_FAILED(rv)) return rv;

  // build resource url for second sort resource
  if (!sortResource2.IsEmpty()) {
    rv = gRDFService->GetUnicodeResource(sortResource2, getter_AddRefs(sortInfo.sortProperty2));
    if (NS_FAILED(rv)) return rv;

    resourceUrl = sortResource2;
    resourceUrl.AppendLiteral("?collation=true");
    rv = gRDFService->GetUnicodeResource(resourceUrl, getter_AddRefs(sortInfo.sortPropertyColl2));
    if (NS_FAILED(rv)) return rv;

    resourceUrl = sortResource2;
    resourceUrl.AppendLiteral("?sort=true");
    rv = gRDFService->GetUnicodeResource(resourceUrl, getter_AddRefs(sortInfo.sortPropertySort2));
    if (NS_FAILED(rv)) return rv;
  }

  // store sort info in attributes on content
  SetSortHints(dbNode, sortResource, sortDirection, sortResource2, sortInfo.inbetweenSeparatorSort, PR_TRUE);

  // start sorting the content
  nsCOMPtr<nsIContent> container;
  if (NS_FAILED(rv = FindSortableContainer(dbNode, getter_AddRefs(container)))) return rv;
  SortContainer(container, &sortInfo, invertTreeFlag);
  
  // Now remove the db node and re-insert it to force the frames to be rebuilt.
  nsCOMPtr<nsIContent> containerParent = container->GetParent();
  PRInt32 containerIndex = containerParent->IndexOf(container);
  PRInt32 childCount = containerParent->GetChildCount();
  if (NS_FAILED(rv = containerParent->RemoveChildAt(containerIndex, PR_TRUE))) return rv;
  if (containerIndex+1 < childCount) {
    if (NS_FAILED(rv = containerParent->InsertChildAt(container, containerIndex, PR_TRUE, PR_TRUE))) return rv;
  } else {
    if (NS_FAILED(rv = containerParent->AppendChildTo(container, PR_TRUE, PR_TRUE))) return rv;
  }
  
  return NS_OK;
}

nsresult
NS_NewXULSortService(nsIXULSortService** mgr)
{
  XULSortServiceImpl *sortService = new XULSortServiceImpl();
  if (!sortService)
    return NS_ERROR_OUT_OF_MEMORY;

  *mgr = sortService;
  NS_ADDREF(*mgr);

  return NS_OK;
}
