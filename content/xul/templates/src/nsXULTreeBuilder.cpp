/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   Ben Goodger <ben@netscape.com>
 *   Jan Varga <varga@ku.sk>
 *   Neil Deakin <enndeakin@sympatico.ca>
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

#include "nscore.h"
#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "nsIDOMElement.h"
#include "nsILocalStore.h"
#include "nsIBoxObject.h"
#include "nsITreeBoxObject.h"
#include "nsITreeSelection.h"
#include "nsITreeColumns.h"
#include "nsITreeView.h"
#include "nsTreeUtils.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsQuickSort.h"
#include "nsTreeRows.h"
#include "nsTemplateRule.h"
#include "nsTemplateMatch.h"
#include "nsGkAtoms.h"
#include "nsXULContentUtils.h"
#include "nsXULTemplateBuilder.h"
#include "nsIXULSortService.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMClassInfo.h"
#include "nsWhitespaceTokenizer.h"
#include "nsTreeContentView.h"

// For security check
#include "nsIDocument.h"

/**
 * A XUL template builder that serves as an tree view, allowing
 * (pretty much) arbitrary RDF to be presented in an tree.
 */
class nsXULTreeBuilder : public nsXULTemplateBuilder,
                         public nsIXULTreeBuilder,
                         public nsINativeTreeView
{
public:
    // nsISupports
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXULTreeBuilder, nsXULTemplateBuilder)

    // nsIXULTreeBuilder
    NS_DECL_NSIXULTREEBUILDER

    // nsITreeView
    NS_DECL_NSITREEVIEW
    // nsINativeTreeView: Untrusted code can use us
    NS_IMETHOD EnsureNative() { return NS_OK; }

    // nsIMutationObserver
    NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

protected:
    friend nsresult
    NS_NewXULTreeBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    nsXULTreeBuilder();

    /**
     * Uninitialize the template builder
     */
    virtual void Uninit(bool aIsFinal);

    /**
     * Get sort variables from the active <treecol>
     */
    nsresult
    EnsureSortVariables();

    virtual nsresult
    RebuildAll();

    /**
     * Given a row, use the row's match to figure out the appropriate
     * <treerow> in the rule's <action>.
     */
    nsresult
    GetTemplateActionRowFor(PRInt32 aRow, nsIContent** aResult);

    /**
     * Given a row and a column ID, use the row's match to figure out
     * the appropriate <treecell> in the rule's <action>.
     */
    nsresult
    GetTemplateActionCellFor(PRInt32 aRow, nsITreeColumn* aCol, nsIContent** aResult);

    /**
     * Return the resource corresponding to a row in the tree.
     */
    nsresult
    GetResourceFor(PRInt32 aRow, nsIRDFResource** aResource);

    /**
     * Open a container row, inserting the container's children into
     * the view.
     */
    nsresult
    OpenContainer(PRInt32 aIndex, nsIXULTemplateResult* aResult);

    /**
     * Helper for OpenContainer, recursively open subtrees, remembering
     * persisted ``open'' state
     */
    nsresult
    OpenSubtreeOf(nsTreeRows::Subtree* aSubtree,
                  PRInt32 aIndex,
                  nsIXULTemplateResult *aResult,
                  PRInt32* aDelta);

    nsresult
    OpenSubtreeForQuerySet(nsTreeRows::Subtree* aSubtree,
                           PRInt32 aIndex,
                           nsIXULTemplateResult *aResult,
                           nsTemplateQuerySet* aQuerySet,
                           PRInt32* aDelta,
                           nsTArray<PRInt32>& open);

    /**
     * Close a container row, removing the container's childrem from
     * the view.
     */
    nsresult
    CloseContainer(PRInt32 aIndex);

    /**
     * Remove the matches for the rows in a subtree
     */
    nsresult
    RemoveMatchesFor(nsTreeRows::Subtree& subtree);

    /**
     * Helper methods that determine if the specified container is open.
     */
    nsresult
    IsContainerOpen(nsIXULTemplateResult *aResult, bool* aOpen);

    nsresult
    IsContainerOpen(nsIRDFResource* aResource, bool* aOpen);

    /**
     * A sorting callback for NS_QuickSort().
     */
    static int
    Compare(const void* aLeft, const void* aRight, void* aClosure);

    /**
     * The real sort routine
     */
    PRInt32
    CompareResults(nsIXULTemplateResult* aLeft, nsIXULTemplateResult* aRight);

    /**
     * Sort the specified subtree, and recursively sort any subtrees
     * beneath it.
     */
    nsresult
    SortSubtree(nsTreeRows::Subtree* aSubtree);

    NS_IMETHOD
    HasGeneratedContent(nsIRDFResource* aResource,
                        nsIAtom* aTag,
                        bool* aGenerated);

    // GetInsertionLocations, ReplaceMatch and SynchronizeResult are inherited
    // from nsXULTemplateBuilder

    /**
     * Return true if the result can be inserted into the template as a new
     * row.
     */
    bool
    GetInsertionLocations(nsIXULTemplateResult* aResult,
                          nsCOMArray<nsIContent>** aLocations);

    /**
     * Implement result replacement
     */
    virtual nsresult
    ReplaceMatch(nsIXULTemplateResult* aOldResult,
                 nsTemplateMatch* aNewMatch,
                 nsTemplateRule* aNewMatchRule,
                 void *aContext);

    /**
     * Implement match synchronization
     */
    virtual nsresult
    SynchronizeResult(nsIXULTemplateResult* aResult);

    /**
     * The tree's box object, used to communicate with the front-end.
     */
    nsCOMPtr<nsITreeBoxObject> mBoxObject;

    /**
     * The tree's selection object.
     */
    nsCOMPtr<nsITreeSelection> mSelection;

    /**
     * The datasource that's used to persist open folder information
     */
    nsCOMPtr<nsIRDFDataSource> mPersistStateStore;

    /**
     * The rows in the view
     */
    nsTreeRows mRows;

    /**
     * The currently active sort variable
     */
    nsCOMPtr<nsIAtom> mSortVariable;

    enum Direction {
        eDirection_Descending = -1,
        eDirection_Natural    =  0,
        eDirection_Ascending  = +1
    };

    /**
     * The currently active sort order
     */
    Direction mSortDirection;

    /*
     * Sort hints (compare case, etc)
     */
    PRUint32 mSortHints;

    /** 
     * The builder observers.
     */
    nsCOMPtr<nsISupportsArray> mObservers;
};

//----------------------------------------------------------------------

nsresult
NS_NewXULTreeBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    *aResult = nsnull;

    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsresult rv;
    nsXULTreeBuilder* result = new nsXULTreeBuilder();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result); // stabilize

    rv = result->InitGlobals();

    if (NS_SUCCEEDED(rv))
        rv = result->QueryInterface(aIID, aResult);

    NS_RELEASE(result);
    return rv;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULTreeBuilder)

NS_IMPL_ADDREF_INHERITED(nsXULTreeBuilder, nsXULTemplateBuilder)
NS_IMPL_RELEASE_INHERITED(nsXULTreeBuilder, nsXULTemplateBuilder)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsXULTreeBuilder, nsXULTemplateBuilder)
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mBoxObject)
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSelection)
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mPersistStateStore)
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mObservers)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

static bool TraverseObservers(nsISupports* aElement, void *aData)
{
    nsCycleCollectionTraversalCallback *cb =
        static_cast<nsCycleCollectionTraversalCallback*>(aData);
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mObservers[i]");
    cb->NoteXPCOMChild(aElement);
    return PR_TRUE;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXULTreeBuilder, nsXULTemplateBuilder)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mBoxObject)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSelection)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPersistStateStore)
    if (tmp->mObservers) {
        tmp->mObservers->EnumerateForwards(TraverseObservers, &cb);
    }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(XULTreeBuilder, nsXULTreeBuilder)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXULTreeBuilder)
    NS_INTERFACE_MAP_ENTRY(nsIXULTreeBuilder)
    NS_INTERFACE_MAP_ENTRY(nsITreeView)
    NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XULTreeBuilder)
NS_INTERFACE_MAP_END_INHERITING(nsXULTemplateBuilder)


nsXULTreeBuilder::nsXULTreeBuilder()
    : mSortDirection(eDirection_Natural), mSortHints(0)
{
}

void
nsXULTreeBuilder::Uninit(bool aIsFinal)
{
    PRInt32 count = mRows.Count();
    mRows.Clear();

    if (mBoxObject) {
        mBoxObject->BeginUpdateBatch();
        mBoxObject->RowCountChanged(0, -count);
        if (mBoxObject) {
            mBoxObject->EndUpdateBatch();
        }
    }

    nsXULTemplateBuilder::Uninit(aIsFinal);
}


//----------------------------------------------------------------------
//
// nsIXULTreeBuilder methods
//

NS_IMETHODIMP
nsXULTreeBuilder::GetResourceAtIndex(PRInt32 aRowIndex, nsIRDFResource** aResult)
{
    if (aRowIndex < 0 || aRowIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    return GetResourceFor(aRowIndex, aResult);
}

NS_IMETHODIMP
nsXULTreeBuilder::GetIndexOfResource(nsIRDFResource* aResource, PRInt32* aResult)
{
    NS_ENSURE_ARG_POINTER(aResource);
    nsTreeRows::iterator iter = mRows.FindByResource(aResource);
    if (iter == mRows.Last())
        *aResult = -1;
    else
        *aResult = iter.GetRowIndex();
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::AddObserver(nsIXULTreeBuilderObserver* aObserver)
{
    nsresult rv;  
    if (!mObservers) {
        rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
        if (NS_FAILED(rv))
            return rv;
    }

    return mObservers->AppendElement(aObserver);
}

NS_IMETHODIMP
nsXULTreeBuilder::RemoveObserver(nsIXULTreeBuilderObserver* aObserver)
{
    return mObservers ? mObservers->RemoveElement(aObserver) : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULTreeBuilder::Sort(nsIDOMElement* aElement)
{
    nsCOMPtr<nsIContent> header = do_QueryInterface(aElement);
    if (! header)
        return NS_ERROR_FAILURE;

    if (header->AttrValueIs(kNameSpaceID_None, nsGkAtoms::sortLocked,
                            nsGkAtoms::_true, eCaseMatters))
        return NS_OK;

    nsAutoString sort;
    header->GetAttr(kNameSpaceID_None, nsGkAtoms::sort, sort);

    if (sort.IsEmpty())
        return NS_OK;

    // Grab the new sort variable
    mSortVariable = do_GetAtom(sort);

    nsAutoString hints;
    header->GetAttr(kNameSpaceID_None, nsGkAtoms::sorthints, hints);

    bool hasNaturalState = true;
    nsWhitespaceTokenizer tokenizer(hints);
    while (tokenizer.hasMoreTokens()) {
      const nsDependentSubstring& token(tokenizer.nextToken());
      if (token.EqualsLiteral("comparecase"))
        mSortHints |= nsIXULSortService::SORT_COMPARECASE;
      else if (token.EqualsLiteral("integer"))
        mSortHints |= nsIXULSortService::SORT_INTEGER;
      else if (token.EqualsLiteral("twostate"))
        hasNaturalState = PR_FALSE;
    }

    // Cycle the sort direction
    nsAutoString dir;
    header->GetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection, dir);

    if (dir.EqualsLiteral("ascending")) {
        dir.AssignLiteral("descending");
        mSortDirection = eDirection_Descending;
    }
    else if (hasNaturalState && dir.EqualsLiteral("descending")) {
        dir.AssignLiteral("natural");
        mSortDirection = eDirection_Natural;
    }
    else {
        dir.AssignLiteral("ascending");
        mSortDirection = eDirection_Ascending;
    }

    // Sort it.
    SortSubtree(mRows.GetRoot());
    mRows.InvalidateCachedRow();
    if (mBoxObject) 
        mBoxObject->Invalidate();

    nsTreeUtils::UpdateSortIndicators(header, dir);

    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsITreeView methods
//

NS_IMETHODIMP
nsXULTreeBuilder::GetRowCount(PRInt32* aRowCount)
{
    *aRowCount = mRows.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetSelection(nsITreeSelection** aSelection)
{
    NS_IF_ADDREF(*aSelection = mSelection.get());
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::SetSelection(nsITreeSelection* aSelection)
{
    NS_ENSURE_TRUE(!aSelection ||
                   nsTreeContentView::CanTrustTreeSelection(aSelection),
                   NS_ERROR_DOM_SECURITY_ERR);
    mSelection = aSelection;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetRowProperties(PRInt32 aIndex, nsISupportsArray* aProperties)
{
    NS_ENSURE_ARG_POINTER(aProperties);
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIContent> row;
    GetTemplateActionRowFor(aIndex, getter_AddRefs(row));
    if (row) {
        nsAutoString raw;
        row->GetAttr(kNameSpaceID_None, nsGkAtoms::properties, raw);

        if (!raw.IsEmpty()) {
            nsAutoString cooked;
            SubstituteText(mRows[aIndex]->mMatch->mResult, raw, cooked);

            nsTreeUtils::TokenizeProperties(cooked, aProperties);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetCellProperties(PRInt32 aRow, nsITreeColumn* aCol, nsISupportsArray* aProperties)
{
    NS_ENSURE_ARG_POINTER(aCol);
    NS_ENSURE_ARG_POINTER(aProperties);
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::properties, raw);

        if (!raw.IsEmpty()) {
            nsAutoString cooked;
            SubstituteText(mRows[aRow]->mMatch->mResult, raw, cooked);

            nsTreeUtils::TokenizeProperties(cooked, aProperties);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetColumnProperties(nsITreeColumn* aCol,
                                      nsISupportsArray* aProperties)
{
    NS_ENSURE_ARG_POINTER(aCol);
    NS_ENSURE_ARG_POINTER(aProperties);
    // XXX sortactive fu
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsContainer(PRInt32 aIndex, bool* aResult)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsTreeRows::iterator iter = mRows[aIndex];

    bool isContainer;
    iter->mMatch->mResult->GetIsContainer(&isContainer);

    iter->mContainerType = isContainer
        ? nsTreeRows::eContainerType_Container
        : nsTreeRows::eContainerType_Noncontainer;

    *aResult = (iter->mContainerType == nsTreeRows::eContainerType_Container);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsContainerOpen(PRInt32 aIndex, bool* aOpen)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsTreeRows::iterator iter = mRows[aIndex];

    if (iter->mContainerState == nsTreeRows::eContainerState_Unknown) {
        bool isOpen;
        IsContainerOpen(iter->mMatch->mResult, &isOpen);

        iter->mContainerState = isOpen
            ? nsTreeRows::eContainerState_Open
            : nsTreeRows::eContainerState_Closed;
    }

    *aOpen = (iter->mContainerState == nsTreeRows::eContainerState_Open);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsContainerEmpty(PRInt32 aIndex, bool* aResult)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsTreeRows::iterator iter = mRows[aIndex];
    NS_ASSERTION(iter->mContainerType == nsTreeRows::eContainerType_Container,
                 "asking for empty state on non-container");

    // if recursion is disabled, pretend that the container is empty. This
    // ensures that folders are still displayed as such, yet won't display
    // their children
    if ((mFlags & eDontRecurse) && (iter->mMatch->mResult != mRootResult)) {
        *aResult = PR_TRUE;
        return NS_OK;
    }

    if (iter->mContainerFill == nsTreeRows::eContainerFill_Unknown) {
        bool isEmpty;
        iter->mMatch->mResult->GetIsEmpty(&isEmpty);

        iter->mContainerFill = isEmpty
            ? nsTreeRows::eContainerFill_Empty
            : nsTreeRows::eContainerFill_Nonempty;
    }

    *aResult = (iter->mContainerFill == nsTreeRows::eContainerFill_Empty);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsSeparator(PRInt32 aIndex, bool* aResult)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsAutoString type;
    nsTreeRows::Row& row = *(mRows[aIndex]);
    row.mMatch->mResult->GetType(type);

    *aResult = type.EqualsLiteral("separator");

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetParentIndex(PRInt32 aRowIndex, PRInt32* aResult)
{
    NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < mRows.Count(), "bad row");
    if (aRowIndex < 0 || aRowIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Construct a path to the row
    nsTreeRows::iterator iter = mRows[aRowIndex];

    // The parent of the row will be at the top of the path
    nsTreeRows::Subtree* parent = iter.GetParent();

    // Now walk through our previous siblings, subtracting off each
    // one's subtree size
    PRInt32 index = iter.GetChildIndex();
    while (--index >= 0)
        aRowIndex -= mRows.GetSubtreeSizeFor(parent, index) + 1;

    // Now the parent's index will be the first row's index, less one.
    *aResult = aRowIndex - 1;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::HasNextSibling(PRInt32 aRowIndex, PRInt32 aAfterIndex, bool* aResult)
{
    NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < mRows.Count(), "bad row");
    if (aRowIndex < 0 || aRowIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Construct a path to the row
    nsTreeRows::iterator iter = mRows[aRowIndex];

    // The parent of the row will be at the top of the path
    nsTreeRows::Subtree* parent = iter.GetParent();

    // We have a next sibling if the child is not the last in the
    // subtree.
    *aResult = iter.GetChildIndex() != parent->Count() - 1;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetLevel(PRInt32 aRowIndex, PRInt32* aResult)
{
    NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < mRows.Count(), "bad row");
    if (aRowIndex < 0 || aRowIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Construct a path to the row; the ``level'' is the path length
    // less one.
    nsTreeRows::iterator iter = mRows[aRowIndex];
    *aResult = iter.GetDepth() - 1;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetImageSrc(PRInt32 aRow, nsITreeColumn* aCol, nsAString& aResult)
{
    NS_ENSURE_ARG_POINTER(aCol);
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad index");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Find the <cell> that corresponds to the column we want.
    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::src, raw);

        SubstituteText(mRows[aRow]->mMatch->mResult, raw, aResult);
    }
    else
        aResult.Truncate();

    return NS_OK;
}


NS_IMETHODIMP
nsXULTreeBuilder::GetProgressMode(PRInt32 aRow, nsITreeColumn* aCol, PRInt32* aResult)
{
    NS_ENSURE_ARG_POINTER(aCol);
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad index");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    *aResult = nsITreeView::PROGRESS_NONE;

    // Find the <cell> that corresponds to the column we want.
    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::mode, raw);

        nsAutoString mode;
        SubstituteText(mRows[aRow]->mMatch->mResult, raw, mode);

        if (mode.EqualsLiteral("normal"))
            *aResult = nsITreeView::PROGRESS_NORMAL;
        else if (mode.EqualsLiteral("undetermined"))
            *aResult = nsITreeView::PROGRESS_UNDETERMINED;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetCellValue(PRInt32 aRow, nsITreeColumn* aCol, nsAString& aResult)
{
    NS_ENSURE_ARG_POINTER(aCol);
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad index");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Find the <cell> that corresponds to the column we want.
    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::value, raw);

        SubstituteText(mRows[aRow]->mMatch->mResult, raw, aResult);
    }
    else
        aResult.Truncate();

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetCellText(PRInt32 aRow, nsITreeColumn* aCol, nsAString& aResult)
{
    NS_ENSURE_ARG_POINTER(aCol);
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad index");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Find the <cell> that corresponds to the column we want.
    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::label, raw);

        SubstituteText(mRows[aRow]->mMatch->mResult, raw, aResult);

    }
    else
        aResult.Truncate();

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::SetTree(nsITreeBoxObject* aTree)
{
    mBoxObject = aTree;

    // If this is teardown time, then we're done.
    if (!mBoxObject) {
        Uninit(PR_FALSE);
        return NS_OK;
    }
    NS_ENSURE_TRUE(mRoot, NS_ERROR_NOT_INITIALIZED);

    // Is our root's principal trusted?
    bool isTrusted = false;
    nsresult rv = IsSystemPrincipal(mRoot->NodePrincipal(), &isTrusted);
    if (NS_SUCCEEDED(rv) && isTrusted) {
        // Get the datasource we intend to use to remember open state.
        nsAutoString datasourceStr;
        mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::statedatasource, datasourceStr);

        // since we are trusted, use the user specified datasource
        // if non specified, use localstore, which gives us
        // persistence across sessions
        if (! datasourceStr.IsEmpty()) {
            gRDFService->GetDataSource(NS_ConvertUTF16toUTF8(datasourceStr).get(),
                                       getter_AddRefs(mPersistStateStore));
        }
        else {
            gRDFService->GetDataSource("rdf:local-store",
                                       getter_AddRefs(mPersistStateStore));
        }
    }

    // Either no specific datasource was specified, or we failed
    // to get one because we are not trusted.
    //
    // XXX if it were possible to ``write an arbitrary datasource
    // back'', then we could also allow an untrusted document to
    // use a statedatasource from the same codebase.
    if (! mPersistStateStore) {
        mPersistStateStore =
            do_CreateInstance("@mozilla.org/rdf/datasource;1?name=in-memory-datasource");
    }

    NS_ASSERTION(mPersistStateStore, "failed to get a persistent state store");
    if (! mPersistStateStore)
        return NS_ERROR_FAILURE;

    Rebuild();

    EnsureSortVariables();
    if (mSortVariable)
        SortSubtree(mRows.GetRoot());

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::ToggleOpenState(PRInt32 aIndex)
{
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsIXULTemplateResult* result = mRows[aIndex]->mMatch->mResult;
    if (! result)
        return NS_ERROR_FAILURE;

    if (mFlags & eDontRecurse)
        return NS_OK;

    if (result && result != mRootResult) {
        // don't open containers if child processing isn't allowed
        bool mayProcessChildren;
        nsresult rv = result->GetMayProcessChildren(&mayProcessChildren);
        if (NS_FAILED(rv) || !mayProcessChildren)
            return rv;
    }

    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer = do_QueryElementAt(mObservers, i);
            if (observer)
                observer->OnToggleOpenState(aIndex);
        }
    }

    if (mPersistStateStore) {
        bool isOpen;
        IsContainerOpen(aIndex, &isOpen);

        nsCOMPtr<nsIRDFResource> container;
        GetResourceFor(aIndex, getter_AddRefs(container));
        if (! container)
            return NS_ERROR_FAILURE;

        bool hasProperty;
        IsContainerOpen(container, &hasProperty);

        if (isOpen) {
            if (hasProperty) {
                mPersistStateStore->Unassert(container,
                                             nsXULContentUtils::NC_open,
                                             nsXULContentUtils::true_);
            }

            CloseContainer(aIndex);
        }
        else {
            if (! hasProperty) {
                mPersistStateStore->Assert(container,
                                           nsXULContentUtils::NC_open,
                                           nsXULContentUtils::true_,
                                           PR_TRUE);
            }

            OpenContainer(aIndex, result);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::CycleHeader(nsITreeColumn* aCol)
{
    NS_ENSURE_ARG_POINTER(aCol);
    nsCOMPtr<nsIDOMElement> element;
    aCol->GetElement(getter_AddRefs(element));

    if (mObservers) {
        nsAutoString id;
        aCol->GetId(id);

        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer = do_QueryElementAt(mObservers, i);
            if (observer)
                observer->OnCycleHeader(id.get(), element);
        }
    }

    return Sort(element);
}

NS_IMETHODIMP
nsXULTreeBuilder::SelectionChanged()
{
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer = do_QueryElementAt(mObservers, i);
            if (observer)
                observer->OnSelectionChanged();
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::CycleCell(PRInt32 aRow, nsITreeColumn* aCol)
{
    NS_ENSURE_ARG_POINTER(aCol);
    if (mObservers) {
        nsAutoString id;
        aCol->GetId(id);

        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer = do_QueryElementAt(mObservers, i);
            if (observer)
                observer->OnCycleCell(aRow, id.get());
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsEditable(PRInt32 aRow, nsITreeColumn* aCol, bool* _retval)
{
    *_retval = PR_TRUE;
    NS_ENSURE_ARG_POINTER(aCol);
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad index");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Find the <cell> that corresponds to the column we want.
    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::editable, raw);

        nsAutoString editable;
        SubstituteText(mRows[aRow]->mMatch->mResult, raw, editable);

        if (editable.EqualsLiteral("false"))
            *_retval = PR_FALSE;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsSelectable(PRInt32 aRow, nsITreeColumn* aCol, bool* _retval)
{
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad index");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    *_retval = PR_TRUE;

    // Find the <cell> that corresponds to the column we want.
    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::selectable, raw);

        nsAutoString selectable;
        SubstituteText(mRows[aRow]->mMatch->mResult, raw, selectable);

        if (selectable.EqualsLiteral("false"))
            *_retval = PR_FALSE;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::SetCellValue(PRInt32 aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
    NS_ENSURE_ARG_POINTER(aCol);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::SetCellText(PRInt32 aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
    NS_ENSURE_ARG_POINTER(aCol);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::PerformAction(const PRUnichar* aAction)
{
    if (mObservers) {  
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer = do_QueryElementAt(mObservers, i);
            if (observer)
                observer->OnPerformAction(aAction);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::PerformActionOnRow(const PRUnichar* aAction, PRInt32 aRow)
{
    if (mObservers) {  
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer = do_QueryElementAt(mObservers, i);
            if (observer)
                observer->OnPerformActionOnRow(aAction, aRow);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::PerformActionOnCell(const PRUnichar* aAction, PRInt32 aRow, nsITreeColumn* aCol)
{
    NS_ENSURE_ARG_POINTER(aCol);
    if (mObservers) {  
        nsAutoString id;
        aCol->GetId(id);

        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer = do_QueryElementAt(mObservers, i);
            if (observer)
                observer->OnPerformActionOnCell(aAction, aRow, id.get());
        }
    }

    return NS_OK;
}


void
nsXULTreeBuilder::NodeWillBeDestroyed(const nsINode* aNode)
{
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
    if (mObservers)
        mObservers->Clear();

    nsXULTemplateBuilder::NodeWillBeDestroyed(aNode);
}

NS_IMETHODIMP
nsXULTreeBuilder::HasGeneratedContent(nsIRDFResource* aResource,
                                      nsIAtom* aTag,
                                      bool* aGenerated)
{
    *aGenerated = PR_FALSE;
    NS_ENSURE_ARG_POINTER(aResource);

    if (!mRootResult)
        return NS_OK;

    nsCOMPtr<nsIRDFResource> rootresource;
    nsresult rv = mRootResult->GetResource(getter_AddRefs(rootresource));
    if (NS_FAILED(rv))
        return rv;

    if (aResource == rootresource ||
        mRows.FindByResource(aResource) != mRows.Last())
        *aGenerated = PR_TRUE;

    return NS_OK;
}

bool
nsXULTreeBuilder::GetInsertionLocations(nsIXULTemplateResult* aResult,
                                        nsCOMArray<nsIContent>** aLocations)
{
    *aLocations = nsnull;

    // Get the reference point and check if it is an open container. Rows
    // should not be generated otherwise.

    nsAutoString ref;
    nsresult rv = aResult->GetBindingFor(mRefVariable, ref);
    if (NS_FAILED(rv) || ref.IsEmpty())
        return PR_FALSE;

    nsCOMPtr<nsIRDFResource> container;
    rv = gRDFService->GetUnicodeResource(ref, getter_AddRefs(container));
    if (NS_FAILED(rv))
        return PR_FALSE;

    // Can always insert into the root resource
    if (container == mRows.GetRootResource())
        return PR_TRUE;

    nsTreeRows::iterator iter = mRows.FindByResource(container);
    if (iter == mRows.Last())
        return PR_FALSE;

    return (iter->mContainerState == nsTreeRows::eContainerState_Open);
}

nsresult
nsXULTreeBuilder::ReplaceMatch(nsIXULTemplateResult* aOldResult,
                               nsTemplateMatch* aNewMatch,
                               nsTemplateRule* aNewMatchRule,
                               void *aLocation)
{
    if (! mBoxObject)
        return NS_OK;

    if (aOldResult) {
        // Grovel through the rows looking for oldresult.
        nsTreeRows::iterator iter = mRows.Find(aOldResult);

        NS_ASSERTION(iter != mRows.Last(), "couldn't find row");
        if (iter == mRows.Last())
            return NS_ERROR_FAILURE;

        // Remove the rows from the view
        PRInt32 row = iter.GetRowIndex();

        // If the row contains children, remove the matches from the
        // children so that they can be regenerated again if the element
        // gets added back.
        PRInt32 delta = mRows.GetSubtreeSizeFor(iter);
        if (delta)
            RemoveMatchesFor(*(iter->mSubtree));

        if (mRows.RemoveRowAt(iter) == 0 && iter.GetRowIndex() >= 0) {

            // In this case iter now points to its parent
            // Invalidate the row's cached fill state
            iter->mContainerFill = nsTreeRows::eContainerFill_Unknown;

            nsCOMPtr<nsITreeColumns> cols;
            mBoxObject->GetColumns(getter_AddRefs(cols));
            if (cols) {
                nsCOMPtr<nsITreeColumn> primaryCol;
                cols->GetPrimaryColumn(getter_AddRefs(primaryCol));
                if (primaryCol)
                    mBoxObject->InvalidateCell(iter.GetRowIndex(), primaryCol);
            }
        }

        // Notify the box object
        mBoxObject->RowCountChanged(row, -delta - 1);
    }

    if (aNewMatch && aNewMatch->mResult) {
        // Insertion.
        PRInt32 row = -1;
        nsTreeRows::Subtree* parent = nsnull;
        nsIXULTemplateResult* result = aNewMatch->mResult;

        nsAutoString ref;
        nsresult rv = result->GetBindingFor(mRefVariable, ref);
        if (NS_FAILED(rv) || ref.IsEmpty())
            return rv;

        nsCOMPtr<nsIRDFResource> container;
        rv = gRDFService->GetUnicodeResource(ref, getter_AddRefs(container));
        if (NS_FAILED(rv))
            return rv;

        if (container != mRows.GetRootResource()) {
            nsTreeRows::iterator iter = mRows.FindByResource(container);
            row = iter.GetRowIndex();

            NS_ASSERTION(iter != mRows.Last(), "couldn't find container row");
            if (iter == mRows.Last())
                return NS_ERROR_FAILURE;

            // Use the persist store to remember if the container
            // is open or closed.
            bool open = false;
            IsContainerOpen(row, &open);

            // If it's open, make sure that we've got a subtree structure ready.
            if (open)
                parent = mRows.EnsureSubtreeFor(iter);

            // We know something has just been inserted into the
            // container, so whether its open or closed, make sure
            // that we've got our tree row's state correct.
            if ((iter->mContainerType != nsTreeRows::eContainerType_Container) ||
                (iter->mContainerFill != nsTreeRows::eContainerFill_Nonempty)) {
                iter->mContainerType  = nsTreeRows::eContainerType_Container;
                iter->mContainerFill = nsTreeRows::eContainerFill_Nonempty;
                mBoxObject->InvalidateRow(iter.GetRowIndex());
            }
        }
        else {
            parent = mRows.GetRoot();
        }

        if (parent) {
            // If we get here, then we're inserting into an open
            // container. By default, place the new element at the
            // end of the container
            PRInt32 index = parent->Count();

            if (mSortVariable) {
                // Figure out where to put the new element by doing an
                // insertion sort.
                PRInt32 left = 0;
                PRInt32 right = index;

                while (left < right) {
                    index = (left + right) / 2;
                    PRInt32 cmp = CompareResults((*parent)[index].mMatch->mResult, result);
                    if (cmp < 0)
                        left = ++index;
                    else if (cmp > 0)
                        right = index;
                    else
                        break;
                }
            }

            nsTreeRows::iterator iter =
                mRows.InsertRowAt(aNewMatch, parent, index);

            mBoxObject->RowCountChanged(iter.GetRowIndex(), +1);

            // See if this newly added row is open; in which case,
            // recursively add its children to the tree, too.

            if (mFlags & eDontRecurse)
                return NS_OK;

            if (result != mRootResult) {
                // don't open containers if child processing isn't allowed
                bool mayProcessChildren;
                nsresult rv = result->GetMayProcessChildren(&mayProcessChildren);
                if (NS_FAILED(rv) || ! mayProcessChildren) return NS_OK;
            }

            bool open;
            IsContainerOpen(result, &open);
            if (open)
                OpenContainer(iter.GetRowIndex(), result);
        }
    }

    return NS_OK;
}

nsresult
nsXULTreeBuilder::SynchronizeResult(nsIXULTemplateResult* aResult)
{
    if (mBoxObject) {
        // XXX we could be more conservative and just invalidate the cells
        // that got whacked...

        nsTreeRows::iterator iter = mRows.Find(aResult);

        NS_ASSERTION(iter != mRows.Last(), "couldn't find row");
        if (iter == mRows.Last())
            return NS_ERROR_FAILURE;

        PRInt32 row = iter.GetRowIndex();
        if (row >= 0)
            mBoxObject->InvalidateRow(row);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p]   => row %d", this, row));
    }

    return NS_OK;
}

//----------------------------------------------------------------------

nsresult
nsXULTreeBuilder::EnsureSortVariables()
{
    // Grovel through <treecols> kids to find the <treecol>
    // with the sort attributes.
    nsCOMPtr<nsIContent> treecols;
 
    nsXULContentUtils::FindChildByTag(mRoot, kNameSpaceID_XUL,
                                      nsGkAtoms::treecols,
                                      getter_AddRefs(treecols));

    if (!treecols)
        return NS_OK;
        
    for (nsIContent* child = treecols->GetFirstChild();
         child;
         child = child->GetNextSibling()) {

        if (child->NodeInfo()->Equals(nsGkAtoms::treecol,
                                      kNameSpaceID_XUL)) {
            if (child->AttrValueIs(kNameSpaceID_None, nsGkAtoms::sortActive,
                                   nsGkAtoms::_true, eCaseMatters)) {
                nsAutoString sort;
                child->GetAttr(kNameSpaceID_None, nsGkAtoms::sort, sort);
                if (! sort.IsEmpty()) {
                    mSortVariable = do_GetAtom(sort);

                    static nsIContent::AttrValuesArray strings[] =
                      {&nsGkAtoms::ascending, &nsGkAtoms::descending, nsnull};
                    switch (child->FindAttrValueIn(kNameSpaceID_None,
                                                   nsGkAtoms::sortDirection,
                                                   strings, eCaseMatters)) {
                       case 0: mSortDirection = eDirection_Ascending; break;
                       case 1: mSortDirection = eDirection_Descending; break;
                       default: mSortDirection = eDirection_Natural; break;
                    }
                }
                break;
            }
        }
    }

    return NS_OK;
}

nsresult
nsXULTreeBuilder::RebuildAll()
{
    NS_ENSURE_TRUE(mRoot, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIDocument> doc = mRoot->GetDocument();

    // Bail out early if we are being torn down.
    if (!doc)
        return NS_OK;

    if (! mQueryProcessor)
        return NS_OK;

    if (mBoxObject) {
        mBoxObject->BeginUpdateBatch();
    }

    if (mQueriesCompiled) {
        Uninit(PR_FALSE);
    }
    else if (mBoxObject) {
        PRInt32 count = mRows.Count();
        mRows.Clear();
        mBoxObject->RowCountChanged(0, -count);
    }

    nsresult rv = CompileQueries();
    if (NS_SUCCEEDED(rv) && mQuerySets.Length() > 0) {
        // Seed the rule network with assignments for the tree row variable
        nsAutoString ref;
        mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::ref, ref);
        if (!ref.IsEmpty()) {
            rv = mQueryProcessor->TranslateRef(mDataSource, ref,
                                               getter_AddRefs(mRootResult));
            if (NS_SUCCEEDED(rv) && mRootResult) {
                OpenContainer(-1, mRootResult);

                nsCOMPtr<nsIRDFResource> rootResource;
                GetResultResource(mRootResult, getter_AddRefs(rootResource));

                mRows.SetRootResource(rootResource);
            }
        }
    }

    if (mBoxObject) {
        mBoxObject->EndUpdateBatch();
    }

    return rv;
}

nsresult
nsXULTreeBuilder::GetTemplateActionRowFor(PRInt32 aRow, nsIContent** aResult)
{
    // Get the template in the DOM from which we're supposed to
    // generate text
    nsTreeRows::Row& row = *(mRows[aRow]);

    // The match stores the indices of the rule and query to use. Use these
    // to look up the right nsTemplateRule and use that rule's action to get
    // the treerow in the template.
    PRInt16 ruleindex = row.mMatch->RuleIndex();
    if (ruleindex >= 0) {
        nsTemplateQuerySet* qs = mQuerySets[row.mMatch->QuerySetPriority()];
        nsTemplateRule* rule = qs->GetRuleAt(ruleindex);
        if (rule) {
            nsCOMPtr<nsIContent> children;
            nsXULContentUtils::FindChildByTag(rule->GetAction(), kNameSpaceID_XUL,
                                              nsGkAtoms::treechildren,
                                              getter_AddRefs(children));
            if (children) {
                nsCOMPtr<nsIContent> item;
                nsXULContentUtils::FindChildByTag(children, kNameSpaceID_XUL,
                                                  nsGkAtoms::treeitem,
                                                  getter_AddRefs(item));
                if (item)
                    return nsXULContentUtils::FindChildByTag(item,
                                                             kNameSpaceID_XUL,
                                                             nsGkAtoms::treerow,
                                                             aResult);
            }
        }
    }

    *aResult = nsnull;
    return NS_OK;
}

nsresult
nsXULTreeBuilder::GetTemplateActionCellFor(PRInt32 aRow,
                                           nsITreeColumn* aCol,
                                           nsIContent** aResult)
{
    *aResult = nsnull;

    if (!aCol) return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIContent> row;
    GetTemplateActionRowFor(aRow, getter_AddRefs(row));
    if (row) {
        nsCOMPtr<nsIAtom> colAtom;
        PRInt32 colIndex;
        aCol->GetAtom(getter_AddRefs(colAtom));
        aCol->GetIndex(&colIndex);

        PRUint32 j = 0;
        for (nsIContent* child = row->GetFirstChild();
             child;
             child = child->GetNextSibling()) {
            
            if (child->NodeInfo()->Equals(nsGkAtoms::treecell,
                                          kNameSpaceID_XUL)) {
                if (colAtom &&
                    child->AttrValueIs(kNameSpaceID_None, nsGkAtoms::ref,
                                       colAtom, eCaseMatters)) {
                    *aResult = child;
                    break;
                }
                else if (j == (PRUint32)colIndex)
                    *aResult = child;
                j++;
            }
        }
    }
    NS_IF_ADDREF(*aResult);

    return NS_OK;
}

nsresult
nsXULTreeBuilder::GetResourceFor(PRInt32 aRow, nsIRDFResource** aResource)
{
    nsTreeRows::Row& row = *(mRows[aRow]);
    return GetResultResource(row.mMatch->mResult, aResource);
}

nsresult
nsXULTreeBuilder::OpenContainer(PRInt32 aIndex, nsIXULTemplateResult* aResult)
{
    // A row index of -1 in this case means ``open tree body''
    NS_ASSERTION(aIndex >= -1 && aIndex < mRows.Count(), "bad row");
    if (aIndex < -1 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsTreeRows::Subtree* container;

    if (aIndex >= 0) {
        nsTreeRows::iterator iter = mRows[aIndex];
        container = mRows.EnsureSubtreeFor(iter.GetParent(),
                                           iter.GetChildIndex());

        iter->mContainerState = nsTreeRows::eContainerState_Open;
    }
    else
        container = mRows.GetRoot();

    if (! container)
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 count;
    OpenSubtreeOf(container, aIndex, aResult, &count);

    // Notify the box object
    if (mBoxObject) {
        if (aIndex >= 0)
            mBoxObject->InvalidateRow(aIndex);

        if (count)
            mBoxObject->RowCountChanged(aIndex + 1, count);
    }

    return NS_OK;
}

nsresult
nsXULTreeBuilder::OpenSubtreeOf(nsTreeRows::Subtree* aSubtree,
                                PRInt32 aIndex,
                                nsIXULTemplateResult *aResult,
                                PRInt32* aDelta)
{
    nsAutoTArray<PRInt32, 8> open;
    PRInt32 count = 0;

    PRInt32 rulecount = mQuerySets.Length();

    for (PRInt32 r = 0; r < rulecount; r++) {
        nsTemplateQuerySet* queryset = mQuerySets[r];
        OpenSubtreeForQuerySet(aSubtree, aIndex, aResult, queryset, &count, open);
    }

    // Now recursively deal with any open sub-containers that just got
    // inserted. We need to do this back-to-front to avoid skewing offsets.
    for (PRInt32 i = open.Length() - 1; i >= 0; --i) {
        PRInt32 index = open[i];

        nsTreeRows::Subtree* child =
            mRows.EnsureSubtreeFor(aSubtree, index);

        nsIXULTemplateResult* result = (*aSubtree)[index].mMatch->mResult;

        PRInt32 delta;
        OpenSubtreeOf(child, aIndex + index, result, &delta);
        count += delta;
    }

    // Sort the container.
    if (mSortVariable) {
        NS_QuickSort(mRows.GetRowsFor(aSubtree),
                     aSubtree->Count(),
                     sizeof(nsTreeRows::Row),
                     Compare,
                     this);
    }

    *aDelta = count;
    return NS_OK;
}

nsresult
nsXULTreeBuilder::OpenSubtreeForQuerySet(nsTreeRows::Subtree* aSubtree,
                                         PRInt32 aIndex,
                                         nsIXULTemplateResult* aResult,
                                         nsTemplateQuerySet* aQuerySet,
                                         PRInt32* aDelta,
                                         nsTArray<PRInt32>& open)
{
    PRInt32 count = *aDelta;
    
    nsCOMPtr<nsISimpleEnumerator> results;
    nsresult rv = mQueryProcessor->GenerateResults(mDataSource, aResult,
                                                   aQuerySet->mCompiledQuery,
                                                   getter_AddRefs(results));
    if (NS_FAILED(rv))
        return rv;

    bool hasMoreResults;
    rv = results->HasMoreElements(&hasMoreResults);

    for (; NS_SUCCEEDED(rv) && hasMoreResults;
           rv = results->HasMoreElements(&hasMoreResults)) {
        nsCOMPtr<nsISupports> nr;
        rv = results->GetNext(getter_AddRefs(nr));
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsIXULTemplateResult> nextresult = do_QueryInterface(nr);
        if (!nextresult)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIRDFResource> resultid;
        rv = GetResultResource(nextresult, getter_AddRefs(resultid));
        if (NS_FAILED(rv))
            return rv;

        if (! resultid)
            continue;

        // check if there is already an existing match. If so, a previous
        // query already generated content so the match is just added to the
        // end of the set of matches.

        bool generateContent = true;

        nsTemplateMatch* prevmatch = nsnull;
        nsTemplateMatch* existingmatch = nsnull;
        if (mMatchMap.Get(resultid, &existingmatch)){
            // check if there is an existing match that matched a rule
            while (existingmatch) {
                if (existingmatch->IsActive())
                    generateContent = PR_FALSE;
                prevmatch = existingmatch;
                existingmatch = existingmatch->mNext;
            }
        }

        nsTemplateMatch *newmatch =
            nsTemplateMatch::Create(mPool, aQuerySet->Priority(),
                                    nextresult, nsnull);
        if (!newmatch)
            return NS_ERROR_OUT_OF_MEMORY;

        if (generateContent) {
            // Don't allow cyclic graphs to get our knickers in a knot.
            bool cyclic = false;

            if (aIndex >= 0) {
                for (nsTreeRows::iterator iter = mRows[aIndex]; iter.GetDepth() > 0; iter.Pop()) {
                    nsCOMPtr<nsIRDFResource> parentid;
                    rv = GetResultResource(iter->mMatch->mResult, getter_AddRefs(parentid));
                    if (NS_FAILED(rv)) {
                        nsTemplateMatch::Destroy(mPool, newmatch, PR_FALSE);
                        return rv;
                    }

                    if (resultid == parentid) {
                        cyclic = PR_TRUE;
                        break;
                    }
                }
            }

            if (cyclic) {
                NS_WARNING("tree cannot handle cyclic graphs");
                nsTemplateMatch::Destroy(mPool, newmatch, PR_FALSE);
                continue;
            }

            PRInt16 ruleindex;
            nsTemplateRule* matchedrule = nsnull;
            rv = DetermineMatchedRule(nsnull, nextresult, aQuerySet,
                                      &matchedrule, &ruleindex);
            if (NS_FAILED(rv)) {
                nsTemplateMatch::Destroy(mPool, newmatch, PR_FALSE);
                return rv;
            }

            if (matchedrule) {
                rv = newmatch->RuleMatched(aQuerySet, matchedrule, ruleindex,
                                           nextresult);
                if (NS_FAILED(rv)) {
                    nsTemplateMatch::Destroy(mPool, newmatch, PR_FALSE);
                    return rv;
                }

                // Remember that this match applied to this row
                mRows.InsertRowAt(newmatch, aSubtree, count);

                // If this is open, then remember it so we can recursively add
                // *its* rows to the tree.
                bool isOpen = false;
                IsContainerOpen(nextresult, &isOpen);
                if (isOpen) {
                    if (open.AppendElement(count) == nsnull)
                        return NS_ERROR_OUT_OF_MEMORY;
                }

                ++count;
            }

            if (mFlags & eLoggingEnabled)
                OutputMatchToLog(resultid, newmatch, PR_TRUE);

        }

        if (prevmatch) {
            prevmatch->mNext = newmatch;
        }
        else if (!mMatchMap.Put(resultid, newmatch)) {
            nsTemplateMatch::Destroy(mPool, newmatch, PR_TRUE);
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    *aDelta = count;
    return rv;
}

nsresult
nsXULTreeBuilder::CloseContainer(PRInt32 aIndex)
{
    NS_ASSERTION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsTreeRows::iterator iter = mRows[aIndex];

    if (iter->mSubtree)
        RemoveMatchesFor(*iter->mSubtree);


    PRInt32 count = mRows.GetSubtreeSizeFor(iter);
    mRows.RemoveSubtreeFor(iter);

    iter->mContainerState = nsTreeRows::eContainerState_Closed;

    if (mBoxObject) {
        mBoxObject->InvalidateRow(aIndex);

        if (count)
            mBoxObject->RowCountChanged(aIndex + 1, -count);
    }

    return NS_OK;
}

nsresult
nsXULTreeBuilder::RemoveMatchesFor(nsTreeRows::Subtree& subtree)
{
    for (PRInt32 i = subtree.Count() - 1; i >= 0; --i) {
        nsTreeRows::Row& row = subtree[i];

        nsTemplateMatch* match = row.mMatch;

        nsCOMPtr<nsIRDFResource> id;
        nsresult rv = GetResultResource(match->mResult, getter_AddRefs(id));
        if (NS_FAILED(rv))
            return rv;

        nsTemplateMatch* existingmatch;
        if (mMatchMap.Get(id, &existingmatch)) {
            while (existingmatch) {
                nsTemplateMatch* nextmatch = existingmatch->mNext;
                nsTemplateMatch::Destroy(mPool, existingmatch, PR_TRUE);
                existingmatch = nextmatch;
            }

            mMatchMap.Remove(id);
        }

        if ((row.mContainerState == nsTreeRows::eContainerState_Open) && row.mSubtree)
            RemoveMatchesFor(*(row.mSubtree));
    }

    return NS_OK;
}

nsresult
nsXULTreeBuilder::IsContainerOpen(nsIXULTemplateResult *aResult, bool* aOpen)
{
    // items are never open if recursion is disabled
    if ((mFlags & eDontRecurse) && aResult != mRootResult) {
        *aOpen = PR_FALSE;
        return NS_OK;
    }

    nsCOMPtr<nsIRDFResource> id;
    nsresult rv = GetResultResource(aResult, getter_AddRefs(id));
    if (NS_FAILED(rv))
        return rv;

    return IsContainerOpen(id, aOpen);
}

nsresult
nsXULTreeBuilder::IsContainerOpen(nsIRDFResource* aResource, bool* aOpen)
{
    if (mPersistStateStore)
        mPersistStateStore->HasAssertion(aResource,
                                         nsXULContentUtils::NC_open,
                                         nsXULContentUtils::true_,
                                         PR_TRUE,
                                         aOpen);
    else
        *aOpen = PR_FALSE;

    return NS_OK;
}

int
nsXULTreeBuilder::Compare(const void* aLeft, const void* aRight, void* aClosure)
{
    nsXULTreeBuilder* self = static_cast<nsXULTreeBuilder*>(aClosure);

    nsTreeRows::Row* left = static_cast<nsTreeRows::Row*>
                                       (const_cast<void*>(aLeft));

    nsTreeRows::Row* right = static_cast<nsTreeRows::Row*>
                                        (const_cast<void*>(aRight));

    return self->CompareResults(left->mMatch->mResult, right->mMatch->mResult);
}

PRInt32
nsXULTreeBuilder::CompareResults(nsIXULTemplateResult* aLeft, nsIXULTemplateResult* aRight)
{
    // this is an extra check done for RDF queries such that results appear in
    // the order they appear in their containing Seq
    if (mSortDirection == eDirection_Natural && mDB) {
        // If the sort order is ``natural'', then see if the container
        // is an RDF sequence. If so, we'll try to use the ordinal
        // properties to determine order.
        //
        // XXX the problem with this is, it doesn't always get the
        // *real* container; e.g.,
        //
        //  <treerow uri="?uri" />
        //
        //  <triple subject="?uri"
        //          predicate="http://home.netscape.com/NC-rdf#subheadings"
        //          object="?subheadings" />
        //
        //  <member container="?subheadings" child="?subheading" />
        //
        // In this case mRefVariable is bound to ?uri, not
        // ?subheadings. (The ``container'' in the template sense !=
        // container in the RDF sense.)

        nsCOMPtr<nsISupports> ref;
        nsresult rv = aLeft->GetBindingObjectFor(mRefVariable, getter_AddRefs(ref));
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsIRDFResource> container = do_QueryInterface(ref);
        if (container) {
            bool isSequence = false;
            gRDFContainerUtils->IsSeq(mDB, container, &isSequence);
            if (isSequence) {
                // Determine the indices of the left and right elements
                // in the container.
                PRInt32 lindex = 0, rindex = 0;

                nsCOMPtr<nsIRDFResource> leftitem;
                aLeft->GetResource(getter_AddRefs(leftitem));
                if (leftitem) {
                    gRDFContainerUtils->IndexOf(mDB, container, leftitem, &lindex);
                    if (lindex < 0)
                        return 0;
                }

                nsCOMPtr<nsIRDFResource> rightitem;
                aRight->GetResource(getter_AddRefs(rightitem));
                if (rightitem) {
                    gRDFContainerUtils->IndexOf(mDB, container, rightitem, &rindex);
                    if (rindex < 0)
                        return 0;
                }

                return lindex - rindex;
            }
        }
    }

    PRInt32 sortorder;
    mQueryProcessor->CompareResults(aLeft, aRight, mSortVariable, mSortHints, &sortorder);

    if (sortorder)
        sortorder = sortorder * mSortDirection;
    return sortorder;
}

nsresult
nsXULTreeBuilder::SortSubtree(nsTreeRows::Subtree* aSubtree)
{
    NS_QuickSort(mRows.GetRowsFor(aSubtree),
                 aSubtree->Count(),
                 sizeof(nsTreeRows::Row),
                 Compare,
                 this);

    for (PRInt32 i = aSubtree->Count() - 1; i >= 0; --i) {
        nsTreeRows::Subtree* child = (*aSubtree)[i].mSubtree;
        if (child)
            SortSubtree(child);
    }

    return NS_OK;
}


/* boolean canDrop (in long index, in long orientation); */
NS_IMETHODIMP
nsXULTreeBuilder::CanDrop(PRInt32 index, PRInt32 orientation,
                          nsIDOMDataTransfer* dataTransfer, bool *_retval)
{
    *_retval = PR_FALSE;
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer = do_QueryElementAt(mObservers, i);
            if (observer) {
                observer->CanDrop(index, orientation, dataTransfer, _retval);
                if (*_retval)
                    break;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::Drop(PRInt32 row, PRInt32 orient, nsIDOMDataTransfer* dataTransfer)
{
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer = do_QueryElementAt(mObservers, i);
            if (observer) {
                bool canDrop = false;
                observer->CanDrop(row, orient, dataTransfer, &canDrop);
                if (canDrop)
                    observer->OnDrop(row, orient, dataTransfer);
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsSorted(bool *_retval)
{
  *_retval = (mSortVariable != nsnull);
  return NS_OK;
}

