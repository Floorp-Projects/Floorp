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
 *   Jan Varga <varga@nixcorp.com>
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

// For sorting
#include "nsICollation.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsCollationCID.h"
#include "nsQuickSort.h"

#include "nsClusterKeySet.h"
#include "nsTreeRows.h"
#include "nsTreeRowTestNode.h"
#include "nsRDFConMemberTestNode.h"
#include "nsTemplateRule.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsXULContentUtils.h"
#include "nsXULTemplateBuilder.h"
#include "nsVoidArray.h"
#include "nsUnicharUtils.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMClassInfo.h"

// For security check
#include "nsIDocument.h"

/**
 * A XUL template builder that serves as an tree view, allowing
 * (pretty much) arbitrary RDF to be presented in an tree.
 */
class nsXULTreeBuilder : public nsXULTemplateBuilder,
                             public nsIXULTreeBuilder,
                             public nsITreeView
{
public:
    // nsISupports
    NS_DECL_ISUPPORTS_INHERITED

    // nsIXULTreeBuilder
    NS_DECL_NSIXULTREEBUILDER

    // nsITreeView
    NS_DECL_NSITREEVIEW

    virtual void DocumentWillBeDestroyed(nsIDocument *aDocument);

protected:
    friend NS_IMETHODIMP
    NS_NewXULTreeBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    nsXULTreeBuilder();
    virtual ~nsXULTreeBuilder();

    /**
     * Initialize the template builder
     */
    nsresult
    Init();

    /**
     * Get sort variables from the active <treecol>
     */
    nsresult
    EnsureSortVariables();

    virtual nsresult
    InitializeRuleNetworkForSimpleRules(InnerNode** aChildNode);

    virtual nsresult
    RebuildAll();

    /**
     * Override default behavior to additionally handle the <row>
     * condition.
     */
    virtual nsresult
    CompileCondition(nsIAtom* aTag,
                     nsTemplateRule* aRule,
                     nsIContent* aCondition,
                     InnerNode* aParentNode,
                     TestNode** aResult);

    /**
     * Compile a <treerow> condition
     */
    nsresult
    CompileTreeRowCondition(nsTemplateRule* aRule,
                                nsIContent* aCondition,
                                InnerNode* aParentNode,
                                TestNode** aResult);

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
     * Return the resource corresponding to a row in the tree. The
     * result is *not* addref'd
     */
    nsIRDFResource*
    GetResourceFor(PRInt32 aRow);

    /**
     * Open a container row, inserting the container's children into
     * the view.
     */
    nsresult
    OpenContainer(PRInt32 aIndex, nsIRDFResource* aContainer);

    /**
     * Helper for OpenContainer, recursively open subtrees, remembering
     * persisted ``open'' state
     */
    nsresult
    OpenSubtreeOf(nsTreeRows::Subtree* aSubtree,
                  PRInt32 aIndex,
                  nsIRDFResource* aContainer,
                  PRInt32* aDelta);

    /**
     * Close a container row, removing the container's childrem from
     * the view.
     */
    nsresult
    CloseContainer(PRInt32 aIndex, nsIRDFResource* aContainer);

    /**
     * Helper for CloseContainer(), recursively remove a subtree from
     * the view. Cleans up the conflict set.
     */
    nsresult
    RemoveMatchesFor(nsIRDFResource* aContainer, nsIRDFResource* aMember);

    /**
     * A helper method that determines if the specified container is open.
     */
    nsresult
    IsContainerOpen(nsIRDFResource* aContainer, PRBool* aResult);

    /**
     * A sorting callback for NS_QuickSort().
     */
    static int PR_CALLBACK
    Compare(const void* aLeft, const void* aRight, void* aClosure);

    /**
     * The real sort routine
     */
    PRInt32
    CompareMatches(nsTemplateMatch* aLeft, nsTemplateMatch* aRight);

    /**
     * Sort the specified subtree, and recursively sort any subtrees
     * beneath it.
     */
    nsresult
    SortSubtree(nsTreeRows::Subtree* aSubtree);

    /**
     * Implement match replacement
     */
    virtual nsresult
    ReplaceMatch(nsIRDFResource* aMember, const nsTemplateMatch* aOldMatch, nsTemplateMatch* aNewMatch);

    /**
     * Implement match synchronization
     */
    virtual nsresult
    SynchronizeMatch(nsTemplateMatch* aMatch, const VariableSet& aModifiedVars);

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
    PRInt32 mSortVariable;

    enum Direction {
        eDirection_Descending = -1,
        eDirection_Natural    =  0,
        eDirection_Ascending  = +1
    };

    /**
     * The currently active sort order
     */
    Direction mSortDirection;

    /**
     * The current collation
     */
    nsCOMPtr<nsICollation> mCollation;

    /** 
     * The builder observers.
     */
    nsCOMPtr<nsISupportsArray> mObservers;
    
    // pseudo-constants
    static PRInt32 gRefCnt;
    static nsIRDFResource* kRDF_type;
    static nsIRDFResource* kNC_BookmarkSeparator;
};
PRInt32         nsXULTreeBuilder::gRefCnt = 0;
nsIRDFResource* nsXULTreeBuilder::kRDF_type;
nsIRDFResource* nsXULTreeBuilder::kNC_BookmarkSeparator;

//----------------------------------------------------------------------

NS_IMETHODIMP
NS_NewXULTreeBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsresult rv;
    nsXULTreeBuilder* result = new nsXULTreeBuilder();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result); // stabilize

    rv = result->Init();

    if (NS_SUCCEEDED(rv))
        rv = result->QueryInterface(aIID, aResult);

    NS_RELEASE(result);
    return rv;
}

NS_IMPL_ADDREF(nsXULTreeBuilder)
NS_IMPL_RELEASE(nsXULTreeBuilder)

NS_INTERFACE_MAP_BEGIN(nsXULTreeBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIXULTreeBuilder)
  NS_INTERFACE_MAP_ENTRY(nsITreeView)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULTreeBuilder)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(XULTreeBuilder)
NS_INTERFACE_MAP_END_INHERITING(nsXULTemplateBuilder)


nsXULTreeBuilder::nsXULTreeBuilder()
    : mSortVariable(0),
      mSortDirection(eDirection_Natural)
{
}

nsresult
nsXULTreeBuilder::Init()
{
    nsresult rv = nsXULTemplateBuilder::Init();
    if (NS_FAILED(rv)) return rv;

    if (gRefCnt++ == 0) {
        gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"), &kRDF_type);
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "BookmarkSeparator"),
                                 &kNC_BookmarkSeparator);
    }

    // Try to acquire a collation object for sorting
    nsCOMPtr<nsILocaleService> ls = do_GetService(NS_LOCALESERVICE_CONTRACTID);
    if (ls) {
        nsCOMPtr<nsILocale> locale;
        ls->GetApplicationLocale(getter_AddRefs(locale));

        if (locale) {
            static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
            nsCOMPtr<nsICollationFactory> cfact =
                do_CreateInstance(kCollationFactoryCID);

            if (cfact)
                cfact->CreateCollation(locale, getter_AddRefs(mCollation));
        }
    }
    return rv;
}

nsXULTreeBuilder::~nsXULTreeBuilder()
{
    if (--gRefCnt == 0) {
        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kNC_BookmarkSeparator);
    }
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

    NS_IF_ADDREF(*aResult = GetResourceFor(aRowIndex));
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetIndexOfResource(nsIRDFResource* aResource, PRInt32* aResult)
{
    nsTreeRows::iterator iter = mRows.Find(mConflictSet, aResource);
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
        if (NS_FAILED(rv)) return rv;
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

    nsAutoString sortLocked;
    header->GetAttr(kNameSpaceID_None, nsXULAtoms::sortLocked, sortLocked);
    if (sortLocked.EqualsLiteral("true"))
        return NS_OK;

    nsAutoString sort;
    header->GetAttr(kNameSpaceID_None, nsXULAtoms::sort, sort);

    if (sort.IsEmpty())
        return NS_OK;

    // Grab the new sort variable
    mSortVariable = mRules.LookupSymbol(sort.get());

    // Cycle the sort direction
    nsAutoString dir;
    header->GetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, dir);

    if (dir.EqualsLiteral("ascending")) {
        dir = NS_LITERAL_STRING("descending");
        mSortDirection = eDirection_Descending;
    }
    else if (dir.EqualsLiteral("descending")) {
        dir = NS_LITERAL_STRING("natural");
        mSortDirection = eDirection_Natural;
    }
    else {
        dir = NS_LITERAL_STRING("ascending");
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
    mSelection = aSelection;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetRowProperties(PRInt32 aIndex, nsISupportsArray* aProperties)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIContent> row;
    GetTemplateActionRowFor(aIndex, getter_AddRefs(row));
    if (row) {
        nsAutoString raw;
        row->GetAttr(kNameSpaceID_None, nsXULAtoms::properties, raw);

        if (!raw.IsEmpty()) {
            nsAutoString cooked;
            SubstituteText(*(mRows[aIndex]->mMatch), raw, cooked);

            nsTreeUtils::TokenizeProperties(cooked, aProperties);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetCellProperties(PRInt32 aRow, nsITreeColumn* aCol, nsISupportsArray* aProperties)
{
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsXULAtoms::properties, raw);

        if (!raw.IsEmpty()) {
            nsAutoString cooked;
            SubstituteText(*(mRows[aRow]->mMatch), raw, cooked);

            nsTreeUtils::TokenizeProperties(cooked, aProperties);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetColumnProperties(nsITreeColumn* aCol,
                                      nsISupportsArray* aProperties)
{
    // XXX sortactive fu
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsContainer(PRInt32 aIndex, PRBool* aResult)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsTreeRows::iterator iter = mRows[aIndex];

    if (iter->mContainerType == nsTreeRows::eContainerType_Unknown) {
        PRBool isContainer;
        CheckContainer(GetResourceFor(aIndex), &isContainer, nsnull);

        iter->mContainerType = isContainer
            ? nsTreeRows::eContainerType_Container
            : nsTreeRows::eContainerType_Noncontainer;
    }

    *aResult = (iter->mContainerType == nsTreeRows::eContainerType_Container);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsContainerOpen(PRInt32 aIndex, PRBool* aResult)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsTreeRows::iterator iter = mRows[aIndex];

    if (iter->mContainerState == nsTreeRows::eContainerState_Unknown) {
        PRBool isOpen;
        IsContainerOpen(GetResourceFor(aIndex), &isOpen);

        iter->mContainerState = isOpen
            ? nsTreeRows::eContainerState_Open
            : nsTreeRows::eContainerState_Closed;
    }

    *aResult = (iter->mContainerState == nsTreeRows::eContainerState_Open);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsContainerEmpty(PRInt32 aIndex, PRBool* aResult)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsTreeRows::iterator iter = mRows[aIndex];
    NS_ASSERTION(iter->mContainerType == nsTreeRows::eContainerType_Container,
                 "asking for empty state on non-container");

    if (iter->mContainerFill == nsTreeRows::eContainerFill_Unknown) {
        PRBool isEmpty;
        CheckContainer(GetResourceFor(aIndex), nsnull, &isEmpty);

        iter->mContainerFill = isEmpty
            ? nsTreeRows::eContainerFill_Empty
            : nsTreeRows::eContainerFill_Nonempty;
    }

    *aResult = (iter->mContainerFill == nsTreeRows::eContainerFill_Empty);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsSeparator(PRInt32 aIndex, PRBool* aResult)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsIRDFResource* resource = GetResourceFor(aIndex);
    mDB->HasAssertion(resource, kRDF_type, kNC_BookmarkSeparator, PR_TRUE, aResult);

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
nsXULTreeBuilder::HasNextSibling(PRInt32 aRowIndex, PRInt32 aAfterIndex, PRBool* aResult)
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
    *aResult = PRBool(iter.GetChildIndex() != parent->Count() - 1);
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
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad index");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Find the <cell> that corresponds to the column we want.
    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, raw);

        SubstituteText(*(mRows[aRow]->mMatch), raw, aResult);
    }
    else
        aResult.SetCapacity(0);

    return NS_OK;
}


NS_IMETHODIMP
nsXULTreeBuilder::GetProgressMode(PRInt32 aRow, nsITreeColumn* aCol, PRInt32* aResult)
{
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad index");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    *aResult = nsITreeView::PROGRESS_NONE;

    // Find the <cell> that corresponds to the column we want.
    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsXULAtoms::mode, raw);

        nsAutoString mode;
        SubstituteText(*(mRows[aRow]->mMatch), raw, mode);

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
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad index");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Find the <cell> that corresponds to the column we want.
    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsXULAtoms::value, raw);

        SubstituteText(*(mRows[aRow]->mMatch), raw, aResult);
    }
    else
        aResult.SetCapacity(0);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetCellText(PRInt32 aRow, nsITreeColumn* aCol, nsAString& aResult)
{
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad index");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Find the <cell> that corresponds to the column we want.
    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aCol, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsXULAtoms::label, raw);

        SubstituteText(*(mRows[aRow]->mMatch), raw, aResult);

    }
    else
        aResult.SetCapacity(0);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::SetTree(nsITreeBoxObject* tree)
{
    NS_PRECONDITION(mRoot, "not initialized");

    mBoxObject = tree;

    // If this is teardown time, then we're done.
    if (! mBoxObject)
        return NS_OK;

    nsCOMPtr<nsIDocument> doc = mRoot->GetDocument();
    NS_ASSERTION(doc, "element has no document");
    if (!doc)
        return NS_ERROR_UNEXPECTED;

    // Grab the doc's principal...
    nsIPrincipal* docPrincipal = doc->GetPrincipal();
    if (!docPrincipal)
        return NS_ERROR_FAILURE;

    PRBool isTrusted = PR_FALSE;
    nsresult rv = IsSystemPrincipal(docPrincipal, &isTrusted);
    if (NS_SUCCEEDED(rv) && isTrusted) {
        // Get the datasource we intend to use to remember open state.
        nsAutoString datasourceStr;
        mRoot->GetAttr(kNameSpaceID_None, nsXULAtoms::statedatasource, datasourceStr);

        // since we are trusted, use the user specified datasource
        // if non specified, use localstore, which gives us
        // persistence across sessions
        if (! datasourceStr.IsEmpty()) {
            gRDFService->GetDataSource(NS_ConvertUCS2toUTF8(datasourceStr).get(),
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
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULTreeBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnToggleOpenState(aIndex);
        }
    }
    
    if (mPersistStateStore) {
        PRBool isOpen;
        IsContainerOpen(aIndex, &isOpen);

        nsIRDFResource* container = GetResourceFor(aIndex);
        if (! container)
            return NS_ERROR_FAILURE;

        PRBool hasProperty;
        IsContainerOpen(container, &hasProperty);

        if (isOpen) {
            if (hasProperty) {
                mPersistStateStore->Unassert(container,
                                             nsXULContentUtils::NC_open,
                                             nsXULContentUtils::true_);
            }

            CloseContainer(aIndex, container);
        }
        else {
            if (! hasProperty) {
                mPersistStateStore->Assert(VALUE_TO_IRDFRESOURCE(container),
                                           nsXULContentUtils::NC_open,
                                           nsXULContentUtils::true_,
                                           PR_TRUE);
            }

            OpenContainer(aIndex, container);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::CycleHeader(nsITreeColumn* aCol)
{
    nsCOMPtr<nsIDOMElement> element;
    aCol->GetElement(getter_AddRefs(element));

    if (mObservers) {
        nsAutoString id;
        aCol->GetId(id);

        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULTreeBuilderObserver), getter_AddRefs(observer));
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
            nsCOMPtr<nsIXULTreeBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULTreeBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnSelectionChanged();
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::CycleCell(PRInt32 row, nsITreeColumn* col)
{
    if (mObservers) {
        nsAutoString id;
        col->GetId(id);

        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULTreeBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnCycleCell(row, id.get());
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsEditable(PRInt32 aRow, nsITreeColumn* aCol, PRBool* _retval)
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
        cell->GetAttr(kNameSpaceID_None, nsXULAtoms::editable, raw);

        nsAutoString editable;
        SubstituteText(*(mRows[aRow]->mMatch), raw, editable);

        if (editable.EqualsLiteral("false"))
            *_retval = PR_FALSE;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::SetCellValue(PRInt32 row, nsITreeColumn* col, const nsAString& value)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::SetCellText(PRInt32 row, nsITreeColumn* col, const nsAString& value)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::PerformAction(const PRUnichar* action)
{
    if (mObservers) {  
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULTreeBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnPerformAction(action);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::PerformActionOnRow(const PRUnichar* action, PRInt32 row)
{
    if (mObservers) {  
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULTreeBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnPerformActionOnRow(action, row);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::PerformActionOnCell(const PRUnichar* action, PRInt32 row, nsITreeColumn* col)
{
    if (mObservers) {  
        nsAutoString id;
        col->GetId(id);

        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULTreeBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnPerformActionOnCell(action, row, id.get());
        }
    }

    return NS_OK;
}


void
nsXULTreeBuilder::DocumentWillBeDestroyed(nsIDocument* aDocument)
{
    if (mObservers)
        mObservers->Clear();

    nsXULTemplateBuilder::DocumentWillBeDestroyed(aDocument);
}

 
nsresult
nsXULTreeBuilder::ReplaceMatch(nsIRDFResource* aMember,
                                   const nsTemplateMatch* aOldMatch,
                                   nsTemplateMatch* aNewMatch)
{
    if (! mBoxObject)
        return NS_OK;

    if (aOldMatch) {
        // Either replacement or removal. Grovel through the rows
        // looking for aOldMatch.
        nsTreeRows::iterator iter = mRows.Find(mConflictSet, aMember);

        NS_ASSERTION(iter != mRows.Last(), "couldn't find row");
        if (iter == mRows.Last())
            return NS_ERROR_FAILURE;

        if (aNewMatch) {
            // replacement
            iter->mMatch = aNewMatch;
            mBoxObject->InvalidateRow(iter.GetRowIndex());
        }
        else {
            // Removal. Clean up the conflict set.
            Value val;
            NS_CONST_CAST(nsTemplateMatch*, aOldMatch)->GetAssignmentFor(mConflictSet, mContainerVar, &val);

            nsIRDFResource* container = VALUE_TO_IRDFRESOURCE(val);
            RemoveMatchesFor(container, aMember);

            // Remove the rows from the view
            PRInt32 row = iter.GetRowIndex();
            PRInt32 delta = mRows.GetSubtreeSizeFor(iter);
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
    }
    else if (aNewMatch) {
        // Insertion.
        Value val;
        aNewMatch->GetAssignmentFor(mConflictSet, mContainerVar, &val);

        nsIRDFResource* container = VALUE_TO_IRDFRESOURCE(val);

        PRInt32 row = -1;
        nsTreeRows::Subtree* parent = nsnull;

        if (container != mRows.GetRootResource()) {
            nsTreeRows::iterator iter =
                mRows.Find(mConflictSet, container);

            row = iter.GetRowIndex();

            NS_ASSERTION(iter != mRows.Last(), "couldn't find container row");
            if (iter == mRows.Last())
                return NS_ERROR_FAILURE;

            // Use the persist store to remember if the container
            // is open or closed.
            PRBool open = PR_FALSE;
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
        else
            parent = mRows.GetRoot();
 
        if (parent) {
            // If we get here, then we're inserting into an open
            // container. By default, place the new element at the
            // end of the container
            PRInt32 index = parent->Count();

            if (mSortVariable) {
                // Figure out where to put the new element by doing an
                // insertion sort.
                PRInt32 left = 0;
                PRInt32 right = parent->Count();

                while (left < right) {
                    index = (left + right) / 2;
                    PRInt32 cmp = CompareMatches((*parent)[index].mMatch, aNewMatch);
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
            Value memberValue;
            aNewMatch->GetAssignmentFor(mConflictSet, mMemberVar, &memberValue);

            nsIRDFResource* member = VALUE_TO_IRDFRESOURCE(memberValue);

            PRBool open;
            IsContainerOpen(member, &open);
            if (open)
                OpenContainer(iter.GetRowIndex(), member);
        }
    }

    return NS_OK;
}

nsresult
nsXULTreeBuilder::SynchronizeMatch(nsTemplateMatch* aMatch, const VariableSet& aModifiedVars)
{
    if (mBoxObject) {
        // XXX we could be more conservative and just invalidate the cells
        // that got whacked...
        Value val;
        aMatch->GetAssignmentFor(mConflictSet, aMatch->mRule->GetMemberVariable(), &val);

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
            nsIRDFResource* res = VALUE_TO_IRDFRESOURCE(val);

            const char* str = "(null)";
            if (res)
                res->GetValueConst(&str);

            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("xultemplate[%p] synchronizing %s (match=%p)", this, str, aMatch));
        }
#endif

        nsTreeRows::iterator iter =
            mRows.Find(mConflictSet, VALUE_TO_IRDFRESOURCE(val));

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
 
    nsXULContentUtils::FindChildByTag(mRoot, kNameSpaceID_XUL, nsXULAtoms::treecols, getter_AddRefs(treecols));

    if (!treecols)
        return NS_OK;

    PRUint32 count = treecols->GetChildCount();
    for (PRUint32 i = 0; i < count; ++i) {
        nsIContent *child = treecols->GetChildAt(i);

        nsINodeInfo *ni = child->GetNodeInfo();
        if (ni && ni->Equals(nsXULAtoms::treecol, kNameSpaceID_XUL)) {
            nsAutoString sortActive;
            child->GetAttr(kNameSpaceID_None, nsXULAtoms::sortActive, sortActive);
            if (sortActive.EqualsLiteral("true")) {
                nsAutoString sort;
                child->GetAttr(kNameSpaceID_None, nsXULAtoms::sort, sort);
                if (!sort.IsEmpty()) {
                    mSortVariable = mRules.LookupSymbol(sort.get(), PR_TRUE);

                    nsAutoString sortDirection;
                    child->GetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, sortDirection);
                    if (sortDirection.EqualsLiteral("ascending"))
                        mSortDirection = eDirection_Ascending;
                    else if (sortDirection.EqualsLiteral("descending"))
                        mSortDirection = eDirection_Descending;
                    else
                        mSortDirection = eDirection_Natural;
                }
                break;
            }
        }
    }

    return NS_OK;
}

nsresult
nsXULTreeBuilder::InitializeRuleNetworkForSimpleRules(InnerNode** aChildNode)
{
    // For simple rules, the rule network will start off looking
    // something like this:
    //
    //   (root)-->(treerow ^id ?a)-->(?a ^member ?b)
    //
    TestNode* rowtestnode =
        new nsTreeRowTestNode(mRules.GetRoot(),
                                  mConflictSet,
                                  mRows,
                                  mContainerVar);

    if (! rowtestnode)
        return NS_ERROR_OUT_OF_MEMORY;

    mRules.GetRoot()->AddChild(rowtestnode);
    mRules.AddNode(rowtestnode);

    // Create (?container ^member ?member)
    nsRDFConMemberTestNode* membernode =
        new nsRDFConMemberTestNode(rowtestnode,
                                   mConflictSet,
                                   mDB,
                                   mContainmentProperties,
                                   mContainerVar,
                                   mMemberVar);

    if (! membernode)
        return NS_ERROR_OUT_OF_MEMORY;

    rowtestnode->AddChild(membernode);
    mRules.AddNode(membernode);

    mRDFTests.Add(membernode);

    *aChildNode = membernode;
    return NS_OK;
}

nsresult
nsXULTreeBuilder::RebuildAll()
{
    NS_PRECONDITION(mRoot != nsnull, "not initialized");
    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsIDocument> doc = mRoot->GetDocument();

    // Bail out early if we are being torn down.
    if (!doc)
        return NS_OK;

    PRInt32 count = mRows.Count();
    mRows.Clear();
    mConflictSet.Clear();

    if (mBoxObject) {
        mBoxObject->BeginUpdateBatch();
        mBoxObject->RowCountChanged(0, -count);
    }

    nsresult rv = CompileRules();
    if (NS_FAILED(rv)) return rv;

    // Seed the rule network with assignments for the tree row
    // variable
    nsCOMPtr<nsIRDFResource> root;
    nsXULContentUtils::GetElementRefResource(mRoot, getter_AddRefs(root));
    mRows.SetRootResource(root);

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* s = "(null)";
        if (root)
            root->GetValueConst(&s);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] root=%s", this, s));
    }
#endif

    if (root)
        OpenContainer(-1, root);

    if (mBoxObject) {
        mBoxObject->EndUpdateBatch();
    }

    return NS_OK;
}

nsresult
nsXULTreeBuilder::CompileCondition(nsIAtom* aTag,
                                       nsTemplateRule* aRule,
                                       nsIContent* aCondition,
                                       InnerNode* aParentNode,
                                       TestNode** aResult)
{
    nsresult rv;

    if (aTag == nsXULAtoms::content || aTag == nsXULAtoms::treeitem)
        rv = CompileTreeRowCondition(aRule, aCondition, aParentNode, aResult);
    else
        rv = nsXULTemplateBuilder::CompileCondition(aTag, aRule, aCondition, aParentNode, aResult);

    return rv;
}

nsresult
nsXULTreeBuilder::CompileTreeRowCondition(nsTemplateRule* aRule,
                                                  nsIContent* aCondition,
                                                  InnerNode* aParentNode,
                                                  TestNode** aResult)
{
    // Compile a <content> condition, which must be of the form:
    //
    //   <content uri="?uri" />
    //
    // Right now, exactly one <row> condition is required per rule. It
    // creates an nsTreeRowTestNode, binding the test's variable
    // to the global row variable that's used during match
    // propagation. The ``uri'' attribute must be set.

    nsAutoString uri;
    aCondition->GetAttr(kNameSpaceID_None, nsXULAtoms::uri, uri);

    if (uri[0] != PRUnichar('?')) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] on <row> test, expected 'uri' attribute to name a variable", this));

        return NS_OK;
    }

    PRInt32 urivar = mRules.LookupSymbol(uri.get());
    if (! urivar) {
        if (mContainerSymbol.IsEmpty()) {
            // If the container symbol was not explictly declared on
            // the <template> tag, or we haven't seen a previous rule
            // whose <content> condition defined it, then we'll
            // implictly define it *now*.
            mContainerSymbol = uri;
            urivar = mContainerVar;
        }
        else
            urivar = mRules.CreateAnonymousVariable();

        mRules.PutSymbol(uri.get(), urivar);
    }

    TestNode* testnode =
        new nsTreeRowTestNode(aParentNode,
                                  mConflictSet,
                                  mRows,
                                  urivar);

    if (! testnode)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = testnode;
    return NS_OK;
}

nsresult
nsXULTreeBuilder::GetTemplateActionRowFor(PRInt32 aRow, nsIContent** aResult)
{
    // Get the template in the DOM from which we're supposed to
    // generate text
    nsTreeRows::Row& row = *(mRows[aRow]);

    nsCOMPtr<nsIContent> action;
    row.mMatch->mRule->GetContent(getter_AddRefs(action));

    nsCOMPtr<nsIContent> children;
    nsXULContentUtils::FindChildByTag(action, kNameSpaceID_XUL, nsXULAtoms::treechildren, getter_AddRefs(children));
    if (children) {
        nsCOMPtr<nsIContent> item;
        nsXULContentUtils::FindChildByTag(children, kNameSpaceID_XUL, nsXULAtoms::treeitem, getter_AddRefs(item));
        if (item)
            return nsXULContentUtils::FindChildByTag(item, kNameSpaceID_XUL, nsXULAtoms::treerow, aResult);
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

    nsCOMPtr<nsIContent> row;
    GetTemplateActionRowFor(aRow, getter_AddRefs(row));
    if (row) {
        const PRUnichar* colID;
        PRInt32 colIndex;
        aCol->GetIdConst(&colID);
        aCol->GetIndex(&colIndex);

        PRUint32 count = row->GetChildCount();
        PRUint32 j = 0;
        for (PRUint32 i = 0; i < count; ++i) {
            nsIContent *child = row->GetChildAt(i);

            nsINodeInfo *ni = child->GetNodeInfo();

            if (ni && ni->Equals(nsXULAtoms::treecell, kNameSpaceID_XUL)) {
                nsAutoString ref;
                child->GetAttr(kNameSpaceID_None, nsXULAtoms::ref, ref);
                if (!ref.IsEmpty() && ref.Equals(colID)) {
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

nsIRDFResource*
nsXULTreeBuilder::GetResourceFor(PRInt32 aRow)
{
    nsTreeRows::Row& row = *(mRows[aRow]);

    Value member;
    row.mMatch->GetAssignmentFor(mConflictSet, mMemberVar, &member);

    return VALUE_TO_IRDFRESOURCE(member); // not refcounted
}

nsresult
nsXULTreeBuilder::OpenContainer(PRInt32 aIndex, nsIRDFResource* aContainer)
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
    OpenSubtreeOf(container, aIndex, aContainer, &count);

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
                                    nsIRDFResource* aContainer,
                                    PRInt32* aDelta)
{
    Instantiation seed;
    seed.AddAssignment(mContainerVar, Value(aContainer));

#ifdef PR_LOGGING
    static PRInt32 gNest;

    nsCAutoString space;
    {
        for (PRInt32 i = 0; i < gNest; ++i)
            space += "  ";
    }

    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* res;
        aContainer->GetValueConst(&res);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] %sopening subtree for %s", this, space.get(), res));
    }

    ++gNest;
#endif

    InstantiationSet instantiations;
    instantiations.Append(seed);

    // Propagate the assignments through the network
    nsClusterKeySet newkeys;
    mRules.GetRoot()->Propagate(instantiations, &newkeys);

    nsAutoVoidArray open;
    PRInt32 count = 0;

    // Iterate through newly added keys to determine which rules fired
    nsClusterKeySet::ConstIterator last = newkeys.Last();
    for (nsClusterKeySet::ConstIterator key = newkeys.First(); key != last; ++key) {
        nsConflictSet::MatchCluster* matches =
            mConflictSet.GetMatchesForClusterKey(*key);

        if (! matches)
            continue;

        nsTemplateMatch* match = 
            mConflictSet.GetMatchWithHighestPriority(matches);

        NS_ASSERTION(match != nsnull, "no best match in match set");
        if (! match)
            continue;

#ifdef PR_LOGGING
        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] %smatch=%p", this, space.get(), match));
#endif

        Value val;
        match->GetAssignmentFor(mConflictSet,
                                match->mRule->GetMemberVariable(),
                                &val);

        // Don't allow cyclic graphs to get our knickers in a knot.
        PRBool cyclic = PR_FALSE;

        if (aIndex >= 0) {
            for (nsTreeRows::iterator iter = mRows[aIndex]; iter.GetDepth() > 0; iter.Pop()) {
                nsTemplateMatch* parentMatch = iter->mMatch;

                Value parentVal;
                parentMatch->GetAssignmentFor(mConflictSet,
                                              parentMatch->mRule->GetMemberVariable(),
                                              &parentVal);

                if (val == parentVal) {
                    cyclic = PR_TRUE;
                    break;
                }
            }
        }

        if (cyclic) {
            NS_WARNING("outliner cannot handle cyclic graphs");
            continue;
        }

        // Remember that this match applied to this row
        mRows.InsertRowAt(match, aSubtree, count);

        // Remember this as the "last" match
        matches->mLastMatch = match;

        // If this is open, then remember it so we can recursively add
        // *its* rows to the tree.
        PRBool isOpen = PR_FALSE;
        IsContainerOpen(VALUE_TO_IRDFRESOURCE(val), &isOpen);
        if (isOpen)
            open.AppendElement((void*) count);

        ++count;
    }

    // Now recursively deal with any open sub-containers that just got
    // inserted. We need to do this back-to-front to avoid skewing offsets.
    for (PRInt32 i = open.Count() - 1; i >= 0; --i) {
        PRInt32 index = NS_PTR_TO_INT32(open[i]);

        nsTreeRows::Subtree* child =
            mRows.EnsureSubtreeFor(aSubtree, index);

        nsTemplateMatch* match = (*aSubtree)[index].mMatch;

        Value val;
        match->GetAssignmentFor(mConflictSet, match->mRule->GetMemberVariable(), &val);

        PRInt32 delta;
        OpenSubtreeOf(child, aIndex + index, VALUE_TO_IRDFRESOURCE(val), &delta);
        count += delta;
    }

#ifdef PR_LOGGING
    --gNest;
#endif

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
nsXULTreeBuilder::CloseContainer(PRInt32 aIndex, nsIRDFResource* aContainer)
{
    NS_ASSERTION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* res;
        aContainer->GetValueConst(&res);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] closing container %s", this, res));
    }
#endif

    nsTemplateMatchSet firings(mConflictSet.GetPool());
    nsTemplateMatchSet retractions(mConflictSet.GetPool());
    mConflictSet.Remove(nsTreeRowTestNode::Element(aContainer), firings, retractions);

    {
        // Clean up the conflict set
        nsTemplateMatchSet::ConstIterator last = retractions.Last();
        nsTemplateMatchSet::ConstIterator iter;

        for (iter = retractions.First(); iter != last; ++iter) {
            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("xultemplate[%p] removing match %p", this, iter.operator->()));

            Value val;
            iter->GetAssignmentFor(mConflictSet, iter->mRule->GetMemberVariable(), &val);

            RemoveMatchesFor(aContainer, VALUE_TO_IRDFRESOURCE(val));
        }
    }

    {
        // Update the view
        nsTreeRows::iterator iter = mRows[aIndex];

        PRInt32 count = mRows.GetSubtreeSizeFor(iter);
        mRows.RemoveSubtreeFor(iter);

        iter->mContainerState = nsTreeRows::eContainerState_Closed;

        if (mBoxObject) {
            mBoxObject->InvalidateRow(aIndex);

            if (count)
                mBoxObject->RowCountChanged(aIndex + 1, -count);
        }
    }

    return NS_OK;
}

nsresult
nsXULTreeBuilder::RemoveMatchesFor(nsIRDFResource* aContainer, nsIRDFResource* aMember)
{
    NS_PRECONDITION(aContainer != nsnull, "null ptr");
    if (! aContainer)
        return NS_ERROR_FAILURE;

    NS_PRECONDITION(aMember != nsnull, "null ptr");
    if (! aMember)
        return NS_ERROR_FAILURE;

#ifdef PR_LOGGING
    static PRInt32 gNest;

    nsCAutoString space;
    for (PRInt32 i = 0; i < gNest; ++i)
        space += "  ";

    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* res;
        aMember->GetValueConst(&res);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] %sremoving matches for %s", this, space.get(), res));
    }

    ++gNest;
#endif

    // Pull supporting memory elements out of the conflict set. We
    // yank the container/member element so that it will be recreated
    // when the container is opened; we yank the row element so we'll
    // recurse to any open children.
    nsTemplateMatchSet firings(mConflictSet.GetPool());
    nsTemplateMatchSet retractions(mConflictSet.GetPool());
    mConflictSet.Remove(nsRDFConMemberTestNode::Element(aContainer, aMember), firings, retractions);
    mConflictSet.Remove(nsTreeRowTestNode::Element(aMember), firings, retractions);

    nsTemplateMatchSet::ConstIterator last = retractions.Last();
    nsTemplateMatchSet::ConstIterator iter;

    for (iter = retractions.First(); iter != last; ++iter) {
#ifdef PR_LOGGING
        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] %smatch=%p", this, space.get(), iter.operator->()));
#endif

        Value val;
        iter->GetAssignmentFor(mConflictSet, iter->mRule->GetMemberVariable(), &val);
        RemoveMatchesFor(aMember, VALUE_TO_IRDFRESOURCE(val));
    }

#ifdef PR_LOGGING
    --gNest;
#endif

    return NS_OK;
}

nsresult
nsXULTreeBuilder::IsContainerOpen(nsIRDFResource* aContainer, PRBool* aResult)
{
    if (mPersistStateStore)
        mPersistStateStore->HasAssertion(aContainer,
                                         nsXULContentUtils::NC_open,
                                         nsXULContentUtils::true_,
                                         PR_TRUE,
                                         aResult);
    else
        *aResult = PR_FALSE;

    return NS_OK;
}

int
nsXULTreeBuilder::Compare(const void* aLeft, const void* aRight, void* aClosure)
{
    nsXULTreeBuilder* self = NS_STATIC_CAST(nsXULTreeBuilder*, aClosure);

    nsTreeRows::Row* left = NS_STATIC_CAST(nsTreeRows::Row*,
                                               NS_CONST_CAST(void*, aLeft));

    nsTreeRows::Row* right = NS_STATIC_CAST(nsTreeRows::Row*,
                                                NS_CONST_CAST(void*, aRight));

    return self->CompareMatches(left->mMatch, right->mMatch);
}

PRInt32
nsXULTreeBuilder::CompareMatches(nsTemplateMatch* aLeft, nsTemplateMatch* aRight)
{
    PRInt32 result = 0;

    if (mSortDirection == eDirection_Natural) {
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
        // In this case mContainerVar is bound to ?uri, not
        // ?subheadings. (The ``container'' in the template sense !=
        // container in the RDF sense.)
        Value val;
        aLeft->GetAssignmentFor(mConflictSet, mContainerVar, &val);

        nsIRDFResource* container = VALUE_TO_IRDFRESOURCE(val);

        PRBool isSequence = PR_FALSE;
        gRDFContainerUtils->IsSeq(mDB, container, &isSequence);
        if (! isSequence)
            // If it's not an RDF container, then there's no natural
            // order.
            return 0;

        // Determine the indices of the left and right elements in the
        // container.
        Value left;
        aLeft->GetAssignmentFor(mConflictSet, mMemberVar, &left);

        PRInt32 lindex;
        gRDFContainerUtils->IndexOf(mDB, container, VALUE_TO_IRDFNODE(left), &lindex);
        if (lindex < 0)
            return 0;

        Value right;
        aRight->GetAssignmentFor(mConflictSet, mMemberVar, &right);

        PRInt32 rindex;
        gRDFContainerUtils->IndexOf(mDB, container, VALUE_TO_IRDFNODE(right), &rindex);
        if (rindex < 0)
            return 0;

        return lindex - rindex;
    }

    // If we get here, then an ascending or descending sort order is
    // imposed.
    Value leftValue;
    aLeft->GetAssignmentFor(mConflictSet, mSortVariable, &leftValue);
    nsIRDFNode* leftNode = VALUE_TO_IRDFNODE(leftValue);

    Value rightValue;
    aRight->GetAssignmentFor(mConflictSet, mSortVariable, &rightValue);
    nsIRDFNode* rightNode = VALUE_TO_IRDFNODE(rightValue);

    {
        // Literals?
        nsCOMPtr<nsIRDFLiteral> l = do_QueryInterface(leftNode);
        if (l) {
            nsCOMPtr<nsIRDFLiteral> r = do_QueryInterface(rightNode);
            if (r) {
                const PRUnichar *lstr, *rstr;
                l->GetValueConst(&lstr);
                r->GetValueConst(&rstr);

                if (mCollation) {
                    mCollation->CompareString(kCollationCaseInSensitive,
                                              nsDependentString(lstr),
                                              nsDependentString(rstr),
                                              &result);
                }
                else
                    result = ::Compare(nsDependentString(lstr),
                                       nsDependentString(rstr),
                                       nsCaseInsensitiveStringComparator());

                return result * mSortDirection;
            }
        }
    }

    {
        // Dates?
        nsCOMPtr<nsIRDFDate> l = do_QueryInterface(leftNode);
        if (l) {
            nsCOMPtr<nsIRDFDate> r = do_QueryInterface(rightNode);
            if (r) {
                PRTime ldate, rdate;
                l->GetValue(&ldate);
                r->GetValue(&rdate);

                PRInt64 delta;
                LL_SUB(delta, ldate, rdate);

                if (LL_IS_ZERO(delta))
                    result = 0;
                else if (LL_GE_ZERO(delta))
                    result = 1;
                else
                    result = -1;

                return result * mSortDirection;
            }
        }
    }

    {
        // Integers?
        nsCOMPtr<nsIRDFInt> l = do_QueryInterface(leftNode);
        if (l) {
            nsCOMPtr<nsIRDFInt> r = do_QueryInterface(rightNode);
            if (r) {
                PRInt32 lval, rval;
                l->GetValue(&lval);
                r->GetValue(&rval);

                result = lval - rval;

                return result * mSortDirection;
            }
        }
    }

    if (mCollation) {
        // Blobs? (We can only compare these reasonably if we have a
        // collation object.)
        nsCOMPtr<nsIRDFBlob> l = do_QueryInterface(leftNode);
        if (l) {
            nsCOMPtr<nsIRDFBlob> r = do_QueryInterface(rightNode);
            if (r) {
                const PRUint8 *lval, *rval;
                PRInt32 llen, rlen;
                l->GetValue(&lval);
                l->GetLength(&llen);
                r->GetValue(&rval);
                r->GetLength(&rlen);
                
                mCollation->CompareRawSortKey(lval, llen, rval, rlen, &result);
                return result * mSortDirection;
            }
        }
    }

    // Ack! Apples & oranges...
    return 0;
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
nsXULTreeBuilder::CanDrop(PRInt32 index, PRInt32 orientation, PRBool *_retval)
{
    *_retval = PR_FALSE;
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULTreeBuilderObserver), getter_AddRefs(observer));
            if (observer) {
                observer->CanDrop(index, orientation, _retval);
                if (*_retval)
                    break;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::Drop(PRInt32 row, PRInt32 orient)
{
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULTreeBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULTreeBuilderObserver), getter_AddRefs(observer));
            if (observer) {
                PRBool canDrop = PR_FALSE;
                observer->CanDrop(row, orient, &canDrop);
                if (canDrop)
                    observer->OnDrop(row, orient);
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsSorted(PRBool *_retval)
{
  *_retval = mSortVariable;
  return NS_OK;
}

