/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chris Waterson <waterson@netscape.com>
 *
 * Notes
 *
 * . GetCellText() grovels through the <action> element's content
 *   model to compute the cell text. We could make this be better.
 */

#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsILocalStore.h"
#include "nsIOutlinerBoxObject.h"
#include "nsIOutlinerSelection.h"
#include "nsIOutlinerView.h"
#include "nsIServiceManager.h"

// For sorting
#include "nsICollation.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsCollationCID.h"
#include "nsQuickSort.h"

#include "nsClusterKeySet.h"
#include "nsOutlinerRows.h"
#include "nsOutlinerRowTestNode.h"
#include "nsRDFConMemberTestNode.h"
#include "nsTemplateRule.h"
#include "nsXULAtoms.h"
#include "nsXULContentUtils.h"
#include "nsXULTemplateBuilder.h"
#include "nsVoidArray.h"

/**
 * A XUL template builder that serves as an outliner view, allowing
 * (pretty much) arbitrary RDF to be presented in an outliner.
 */
class nsXULOutlinerBuilder : public nsXULTemplateBuilder,
                             public nsIXULOutlinerBuilder,
                             public nsIOutlinerView
{
public:
    // nsISupports
    NS_DECL_ISUPPORTS_INHERITED

    // nsIXULOutlinerBuilder
    NS_DECL_NSIXULOUTLINERBUILDER

    // nsIOutlinerView
    NS_DECL_NSIOUTLINERVIEW

    // nsXULTemplateBuilder
    NS_IMETHOD Rebuild();

protected:
    friend NS_IMETHODIMP
    NS_NewXULOutlinerBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    nsXULOutlinerBuilder();
    virtual ~nsXULOutlinerBuilder();

    /**
     * Initialize the template builder
     */
    nsresult
    Init();

    /**
     * Collect sort variables from the <outlinercol>s
     */
    nsresult
    GetSortVariables(VariableSet& aVariables);

    virtual nsresult
    InitializeRuleNetworkForSimpleRules(InnerNode** aChildNode);

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
     * Compile a <outlinerrow> condition
     */
    nsresult
    CompileOutlinerRowCondition(nsTemplateRule* aRule,
                                nsIContent* aCondition,
                                InnerNode* aParentNode,
                                TestNode** aResult);

    /**
     * Given a row, use the row's match to figure out the appropriate
     * <outlinerrow> in the rule's <action>.
     */
    nsresult
    GetTemplateActionRowFor(PRInt32 aRow, nsIContent** aResult);

    /**
     * Given a row and a column ID, use the row's match to figure out
     * the appropriate <outlinercell> in the rule's <action>.
     */
    nsresult
    GetTemplateActionCellFor(PRInt32 aRow, const PRUnichar* aColID, nsIContent** aResult);

    /**
     * Parse a whitespace separated list of properties into an array
     * of atoms.
     */
    nsresult
    TokenizeProperties(const nsAReadableString& aString, nsISupportsArray* aProprerties);

    /**
     * Return the resource corresponding to a row in the outliner. The
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
    OpenSubtreeOf(nsOutlinerRows::Subtree* aSubtree,
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
     * the view.
     */
    nsresult
    RemoveMatchesFor(nsIRDFResource* aContainer);

    /**
     * A helper method that determines if the specified container is open.
     */
    nsresult
    IsContainerOpen(nsIRDFResource* aContainer, PRBool* aResult);

    /**
     * A sorting callback for NS_QuickSort().
     */
    static int
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
    SortSubtree(nsOutlinerRows::Subtree* aSubtree);

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
     * The outliner's box object, used to communicate with the front-end.
     */
    nsCOMPtr<nsIOutlinerBoxObject> mBoxObject;

    /**
     * The outliner's selection object.
     */
    nsCOMPtr<nsIOutlinerSelection> mSelection;

    /**
     * The datasource that's used to persist open folder information
     */
    nsCOMPtr<nsIRDFDataSource> mPersistStateStore;

    /**
     * The rows in the view
     */
    nsOutlinerRows mRows;

    /**
     * The currently active sort variable
     */
    PRInt32 mSortVariable;

    enum Direction {
        eDirection_Descending = -1,
        eDirection_None       =  0,
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
};

//----------------------------------------------------------------------

NS_IMETHODIMP
NS_NewXULOutlinerBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsresult rv;
    nsXULOutlinerBuilder* result = new nsXULOutlinerBuilder();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result); // stabilize

    rv = result->Init();

    if (NS_SUCCEEDED(rv))
        rv = result->QueryInterface(aIID, aResult);

    NS_RELEASE(result);
    return rv;
}

NS_IMPL_ISUPPORTS_INHERITED2(nsXULOutlinerBuilder, nsXULTemplateBuilder,
                             nsIXULOutlinerBuilder,
                             nsIOutlinerView)

nsXULOutlinerBuilder::nsXULOutlinerBuilder()
    : mSortVariable(0),
      mSortDirection(eDirection_None)
{
}

nsresult
nsXULOutlinerBuilder::Init()
{
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

    return nsXULTemplateBuilder::Init();
}

nsXULOutlinerBuilder::~nsXULOutlinerBuilder()
{
}

//----------------------------------------------------------------------
//
// nsIXULOutlinerBuilder methods
//

NS_IMETHODIMP
nsXULOutlinerBuilder::GetResourceFor(PRInt32 aRowIndex, nsIRDFResource** aResult)
{
    if (aRowIndex < 0 || aRowIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    *aResult = GetResourceFor(aRowIndex);
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIOutlinerView methods
//

NS_IMETHODIMP
nsXULOutlinerBuilder::GetRowCount(PRInt32* aRowCount)
{
    *aRowCount = mRows.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::GetSelection(nsIOutlinerSelection** aSelection)
{
    NS_IF_ADDREF(*aSelection = mSelection.get());
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::SetSelection(nsIOutlinerSelection* aSelection)
{
    mSelection = aSelection;
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::GetRowProperties(PRInt32 aIndex, nsISupportsArray* aProperties)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIContent> row;
    GetTemplateActionRowFor(aIndex, getter_AddRefs(row));
    if (row) {
        nsAutoString raw;
        row->GetAttribute(kNameSpaceID_None, nsXULAtoms::properties, raw);

        if (raw.Length()) {
            nsAutoString cooked;
            SubstituteText(*(mRows[aIndex]->mMatch), raw, cooked);

            TokenizeProperties(cooked, aProperties);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::GetCellProperties(PRInt32 aRow, const PRUnichar* aColID, nsISupportsArray* aProperties)
{
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aColID, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttribute(kNameSpaceID_None, nsXULAtoms::properties, raw);

        if (raw.Length()) {
            nsAutoString cooked;
            SubstituteText(*(mRows[aRow]->mMatch), raw, cooked);

            TokenizeProperties(cooked, aProperties);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::GetColumnProperties(const PRUnichar* aColID,
                                          nsIDOMElement* aColElt,
                                          nsISupportsArray* aProperties)
{
    // XXX sortactive fu
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::IsContainer(PRInt32 aIndex, PRBool* aResult)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    return CheckContainer(GetResourceFor(aIndex), aResult, nsnull);
}

NS_IMETHODIMP
nsXULOutlinerBuilder::IsContainerOpen(PRInt32 aIndex, PRBool* aResult)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    return IsContainerOpen(GetResourceFor(aIndex), aResult);
}

NS_IMETHODIMP
nsXULOutlinerBuilder::IsContainerEmpty(PRInt32 aIndex, PRBool* aResult)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    return CheckContainer(GetResourceFor(aIndex), nsnull, aResult);
}

NS_IMETHODIMP
nsXULOutlinerBuilder::GetParentIndex(PRInt32 aRowIndex, PRInt32* aResult)
{
    NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < mRows.Count(), "bad row");
    if (aRowIndex < 0 || aRowIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Construct a path to the row
    nsOutlinerRows::iterator iter = mRows[aRowIndex];

    // The parent of the row will be at the top of the path
    nsOutlinerRows::Subtree* parent = iter.GetParent();

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
nsXULOutlinerBuilder::HasNextSibling(PRInt32 aRowIndex, PRInt32 aAfterIndex, PRBool* aResult)
{
    NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < mRows.Count(), "bad row");
    if (aRowIndex < 0 || aRowIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Construct a path to the row
    nsOutlinerRows::iterator iter = mRows[aRowIndex];

    // The parent of the row will be at the top of the path
    nsOutlinerRows::Subtree* parent = iter.GetParent();

    // We have a next sibling if the child is not the last in the
    // subtree.
    *aResult = PRBool(iter.GetChildIndex() != parent->Count() - 1);
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::GetLevel(PRInt32 aRowIndex, PRInt32* aResult)
{
    NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < mRows.Count(), "bad row");
    if (aRowIndex < 0 || aRowIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Construct a path to the row; the ``level'' is the path length
    // less one.
    nsOutlinerRows::iterator iter = mRows[aRowIndex];
    *aResult = iter.GetDepth() - 1;
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::GetCellText(PRInt32 aRow, const PRUnichar* aColID, PRUnichar** aResult)
{
    NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad index");
    if (aRow < 0 || aRow >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    // Find the <cell> that corresponds to the column we want.
    nsCOMPtr<nsIContent> cell;
    GetTemplateActionCellFor(aRow, aColID, getter_AddRefs(cell));
    if (cell) {
        nsAutoString raw;
        cell->GetAttribute(kNameSpaceID_None, nsXULAtoms::value, raw);

        nsAutoString cooked;
        SubstituteText(*(mRows[aRow]->mMatch), raw, cooked);

        *aResult = nsCRT::strdup(cooked.get());
    }
    else
        *aResult = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::SetOutliner(nsIOutlinerBoxObject* outliner)
{
    mBoxObject = outliner;

    // XXX seems like there's some frame churn going on here, so we'll
    // only try to grab the root element if we don't have it
    // already. (It better not change!)

    if (! mRoot) {
        // Get our root element
        nsCOMPtr<nsIDOMElement> body;
        mBoxObject->GetOutlinerBody(getter_AddRefs(body));

        mRoot = do_QueryInterface(body);

        LoadDataSources();

        // So we can remember open state.
        //
        // XXX We should fix this so that if the document is
        // ``trusted'', we use the localstore.
        mPersistStateStore =
            do_CreateInstance("@mozilla.org/rdf/datasource;1?name=in-memory-datasource");

        if (! mPersistStateStore)
            return NS_ERROR_FAILURE;

        Rebuild();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::ToggleOpenState(PRInt32 aIndex)
{
    if (mPersistStateStore) {
        nsIRDFResource* container = GetResourceFor(aIndex);
        if (! container)
            return NS_ERROR_FAILURE;

        PRBool open;
        IsContainerOpen(container, &open);

        if (open) {
            mPersistStateStore->Unassert(container,
                                         nsXULContentUtils::NC_open,
                                         nsXULContentUtils::true_);

            CloseContainer(aIndex, container);
        }
        else {
            mPersistStateStore->Assert(VALUE_TO_IRDFRESOURCE(container),
                                       nsXULContentUtils::NC_open,
                                       nsXULContentUtils::true_,
                                       PR_TRUE);

            OpenContainer(aIndex, container);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::CycleHeader(const PRUnichar* aColID, nsIDOMElement* aElement)
{
    nsCOMPtr<nsIContent> header = do_QueryInterface(aElement);
    if (! header)
        return NS_ERROR_FAILURE;

    nsAutoString sort;
    header->GetAttribute(kNameSpaceID_None, nsXULAtoms::sort, sort);

    if (sort.Length()) {
        // Grab the new sort variable
        mSortVariable = mRules.LookupSymbol(sort.get());

        // Cycle the sort direction
        nsAutoString dir;
        header->GetAttribute(kNameSpaceID_None, nsXULAtoms::sortDirection, dir);

        if (dir == NS_LITERAL_STRING("ascending")) {
            dir = NS_LITERAL_STRING("descending");
            mSortDirection = eDirection_Descending;
        }
        else if (dir == NS_LITERAL_STRING("descending")) {
            dir = NS_LITERAL_STRING("natural");
            mSortDirection = eDirection_None;
        }
        else {
            dir = NS_LITERAL_STRING("ascending");
            mSortDirection = eDirection_Ascending;
        }

        header->SetAttribute(kNameSpaceID_None, nsXULAtoms::sortDirection, dir, PR_FALSE);

        // Sort it
        SortSubtree(mRows.GetRoot());
        mRows.InvalidateCachedRow();
        mBoxObject->Invalidate();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::SelectionChanged()
{
    // XXX do we care?
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::CycleCell(PRInt32 row, const PRUnichar* colID)
{
    // XXX do we care?
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::IsEditable(PRInt32 row, const PRUnichar* colID, PRBool* _retval)
{
    // XXX Oy. Don't make me do this.
    *_retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::SetCellText(PRInt32 row, const PRUnichar* colID, const PRUnichar* value)
{
    // XXX ...or this.
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::PerformAction(const PRUnichar* action)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::PerformActionOnRow(const PRUnichar* action, PRInt32 row)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::PerformActionOnCell(const PRUnichar* action, PRInt32 row, const PRUnichar* colID)
{
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIXULTemplateBuilder methods
//

NS_IMETHODIMP
nsXULOutlinerBuilder::Rebuild()
{
    NS_PRECONDITION(mRoot != nsnull, "not initialized");
    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    mRows.Clear();
    mConflictSet.Clear();

    rv = CompileRules();
    if (NS_FAILED(rv)) return rv;

    // Seed the rule network with assignments for the outliner row
    // variable
    nsCOMPtr<nsIRDFResource> root;
    nsXULContentUtils::GetElementRefResource(mRoot, getter_AddRefs(root));
    mRows.SetRootResource(root);

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* s;
        root->GetValueConst(&s);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] root=%s", this, s));
    }
#endif

    if (root)
        OpenContainer(-1, root);

    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsXULTemplateBuilder abstract methods
//

nsresult
nsXULOutlinerBuilder::ReplaceMatch(nsIRDFResource* aMember,
                                   const nsTemplateMatch* aOldMatch,
                                   nsTemplateMatch* aNewMatch)
{
    if (! mBoxObject)
        return NS_OK;

    if (aOldMatch) {
        // Either replacement or removal. Grovel through the rows
        // looking for aOldMatch.
        nsOutlinerRows::iterator iter = mRows.Find(mConflictSet, aMember);

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
            RemoveMatchesFor(aMember);

            // Remove the rows from the view
            PRInt32 row = iter.GetRowIndex();
            PRInt32 delta = mRows.GetSubtreeSizeFor(iter);
            mRows.RemoveRowAt(iter);

            // Notify the box object
            mBoxObject->RowCountChanged(row, -delta);
        }
    }
    else if (aNewMatch) {
        // Insertion.
        Value val;
        aNewMatch->GetAssignmentFor(mConflictSet, mContainerVar, &val);

        nsIRDFResource* container = VALUE_TO_IRDFRESOURCE(val);

        PRInt32 row = 0;
        nsOutlinerRows::Subtree* parent = nsnull;

        if (container != mRows.GetRootResource()) {
            nsOutlinerRows::iterator iter =
                mRows.Find(mConflictSet, container);

            row = iter.GetRowIndex();

            NS_ASSERTION(iter != mRows.Last(), "couldn't find container row");
            if (iter == mRows.Last())
                return NS_ERROR_FAILURE;

            // Use the persist store to remember if the container
            // is open or closed.
            PRBool open = PR_FALSE;
            mPersistStateStore->HasAssertion(container,
                                             nsXULContentUtils::NC_open,
                                             nsXULContentUtils::true_,
                                             PR_TRUE,
                                             &open);
            if (open)
                parent = mRows.EnsureSubtreeFor(iter);
        }
        else
            parent = mRows.GetRoot();

        if (parent) {
            // By default, place the new element at the end of the container
            PRInt32 index = parent->Count();

            if (mSortVariable && mSortDirection != eDirection_None) {
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
                        right = index - 1;
                    else
                        break;
                }
            }

            mRows.InsertRowAt(aNewMatch, parent, index);
            mBoxObject->RowCountChanged(row + index, +1);
        }
    }

    return NS_OK;
}

nsresult
nsXULOutlinerBuilder::SynchronizeMatch(nsTemplateMatch* aMatch, const VariableSet& aModifiedVars)
{
    if (mBoxObject) {
        // XXX we could be more conservative and just invalidate the cells
        // that got whacked...
        Value val;
        aMatch->GetAssignmentFor(mConflictSet, aMatch->mRule->GetMemberVariable(), &val);

        nsOutlinerRows::iterator iter =
            mRows.Find(mConflictSet, VALUE_TO_IRDFRESOURCE(val));

        NS_ASSERTION(iter != mRows.Last(), "couldn't find row");
        if (iter == mRows.Last())
            return NS_ERROR_FAILURE;

        PRInt32 row = iter.GetRowIndex();
        if (row >= 0)
            mBoxObject->InvalidateRow(row);
    }

    return NS_OK;
}

//----------------------------------------------------------------------

nsresult
nsXULOutlinerBuilder::GetSortVariables(VariableSet& aVariables)
{
    // The <outlinercol>'s are siblings of the <outlinerbody>, which
    // is what our mRoot is. Walk up one level to find the <outliner>,
    // then grovel through *its* kids to find the <outlinercol>'s...
    nsCOMPtr<nsIContent> outliner;
    mRoot->GetParent(*getter_AddRefs(outliner));

    if (! outliner)
        return NS_ERROR_FAILURE;

    PRInt32 count;
    outliner->ChildCount(count);

    for (PRInt32 i = count - 1; i >= 0; --i) {
        nsCOMPtr<nsIContent> child;
        outliner->ChildAt(i, *getter_AddRefs(child));

        if (! child)
            continue;

        // XXX probably need to get base tag here
        nsCOMPtr<nsIAtom> tag;
        child->GetTag(*getter_AddRefs(tag));

        if (tag != nsXULAtoms::outlinercol)
            continue;

        nsAutoString sort;
        child->GetAttribute(kNameSpaceID_None, nsXULAtoms::sort, sort);
        if (! sort.Length())
            continue;

        PRInt32 var = mRules.LookupSymbol(sort.get(), PR_TRUE);
        aVariables.Add(var);

        nsAutoString active;
        child->GetAttribute(kNameSpaceID_None, nsXULAtoms::sortActive, active);
        if (active == NS_LITERAL_STRING("true")) {
            nsAutoString dir;
            child->GetAttribute(kNameSpaceID_None, nsXULAtoms::sortDirection, dir);

            if (dir == NS_LITERAL_STRING("none"))
                mSortDirection = eDirection_None;
            else if (dir == NS_LITERAL_STRING("descending"))
                mSortDirection = eDirection_Descending;
            else
                mSortDirection = eDirection_Ascending;

            mSortVariable = var;
        }
    }

    return NS_OK;
}

nsresult
nsXULOutlinerBuilder::InitializeRuleNetworkForSimpleRules(InnerNode** aChildNode)
{
    // For simple rules, the rule network will start off looking
    // something like this:
    //
    //   (root)-->(outlinerrow ^id ?a)-->(?a ^member ?b)
    //
    TestNode* rowtestnode =
        new nsOutlinerRowTestNode(mRules.GetRoot(),
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
nsXULOutlinerBuilder::CompileCondition(nsIAtom* aTag,
                                       nsTemplateRule* aRule,
                                       nsIContent* aCondition,
                                       InnerNode* aParentNode,
                                       TestNode** aResult)
{
    nsresult rv;

    if (aTag == nsXULAtoms::outlinerrow)
        rv = CompileOutlinerRowCondition(aRule, aCondition, aParentNode, aResult);
    else
        rv = nsXULTemplateBuilder::CompileCondition(aTag, aRule, aCondition, aParentNode, aResult);

    return rv;
}

nsresult
nsXULOutlinerBuilder::CompileOutlinerRowCondition(nsTemplateRule* aRule,
                                                  nsIContent* aCondition,
                                                  InnerNode* aParentNode,
                                                  TestNode** aResult)
{
    // Compile a <outlinerrow> condition, which must be of the form:
    //
    //   <outlinerrow uri="?uri" />
    //
    // Right now, exactly one <row> condition is required per rule. It
    // creates an nsOutlinerRowTestNode, binding the test's variable
    // to the global row variable that's used during match
    // propogation. The ``uri'' attribute must be set.

    nsAutoString uri;
    aCondition->GetAttribute(kNameSpaceID_None, nsXULAtoms::uri, uri);

    if (uri[0] != PRUnichar('?')) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] on <row> test, expected 'uri' attribute to name a variable", this));

        return NS_OK;
    }

    PRInt32 urivar = mRules.LookupSymbol(uri.get());
    if (! urivar) {
        if (! mContainerSymbol.Length()) {
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
        new nsOutlinerRowTestNode(aParentNode,
                                  mConflictSet,
                                  mRows,
                                  urivar);

    if (! testnode)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = testnode;
    return NS_OK;
}

nsresult
nsXULOutlinerBuilder::GetTemplateActionRowFor(PRInt32 aRow, nsIContent** aResult)
{
    // Get the template in the DOM from which we're supposed to
    // generate text
    nsOutlinerRows::Row& row = *(mRows[aRow]);

    nsCOMPtr<nsIContent> action;
    row.mMatch->mRule->GetContent(getter_AddRefs(action));

    return nsXULContentUtils::FindChildByTag(action, kNameSpaceID_XUL,
                                             nsXULAtoms::outlinerrow,
                                             aResult);
}

nsresult
nsXULOutlinerBuilder::GetTemplateActionCellFor(PRInt32 aRow,
                                               const PRUnichar* aColID,
                                               nsIContent** aResult)
{
    nsCOMPtr<nsIContent> row;
    GetTemplateActionRowFor(aRow, getter_AddRefs(row));
    if (row) {
        PRInt32 count;
        row->ChildCount(count);

        for (PRInt32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIContent> child;
            row->ChildAt(i, *getter_AddRefs(child));

            if (! child)
                continue;

            // XXX get base tag here?
            nsCOMPtr<nsIAtom> tag;
            child->GetTag(*getter_AddRefs(tag));

            if (tag != nsXULAtoms::outlinercell)
                continue;

            nsAutoString ref;
            child->GetAttribute(kNameSpaceID_None, nsXULAtoms::ref, ref);

            if (ref.Equals(aColID)) {
                NS_ADDREF(*aResult = child.get());
                return NS_OK;
            }
        }
    }

    *aResult = nsnull;
    return NS_OK;
}

nsresult
nsXULOutlinerBuilder::TokenizeProperties(const nsAReadableString& aString,
                                         nsISupportsArray* aProperties)
{
    NS_PRECONDITION(aProperties != nsnull, "null ptr");
    if (! aProperties)
        return NS_ERROR_NULL_POINTER;

    nsAReadableString::const_iterator end;
    aString.EndReading(end);

    nsAReadableString::const_iterator iter;
    aString.BeginReading(iter);

    do {
        // Skip whitespace
        while (iter != end && nsCRT::IsAsciiSpace(*iter))
            ++iter;

        // If only whitespace, we're done
        if (iter == end)
            break;

        // Note the first non-whitespace character
        nsAReadableString::const_iterator first = iter;

        // Advance to the next whitespace character
        while (iter != end && ! nsCRT::IsAsciiSpace(*iter))
            ++iter;

        // XXX this would be nonsensical
        NS_ASSERTION(iter != first, "eh? something's wrong here");
        if (iter == first)
            break;

     
#if 1 // XXX until bug 55143 is fixed, we need to copy so the string
      // is zero-terminated
        nsAutoString s(Substring(first, iter));
        nsCOMPtr<nsIAtom> atom = dont_AddRef(NS_NewAtom(s));
#else
        nsCOMPtr<nsIAtom> atom = dont_AddRef(NS_NewAtom(Substring(first, iter)));
#endif
        aProperties->AppendElement(atom);
    } while (iter != end);

    return NS_OK;
}

nsIRDFResource*
nsXULOutlinerBuilder::GetResourceFor(PRInt32 aRow)
{
    nsOutlinerRows::Row& row = *(mRows[aRow]);

    Value member;
    row.mMatch->GetAssignmentFor(mConflictSet, mMemberVar, &member);

    return VALUE_TO_IRDFRESOURCE(member); // not refcounted
}

nsresult
nsXULOutlinerBuilder::OpenContainer(PRInt32 aIndex, nsIRDFResource* aContainer)
{
    // A row index of -1 in this case means ``open outliner body''
    NS_ASSERTION(aIndex >= -1 && aIndex < mRows.Count(), "bad row");
    if (aIndex < -1 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsOutlinerRows::Subtree* container;

    if (aIndex >= 0) {
        nsOutlinerRows::iterator iter = mRows[aIndex];
        container = mRows.EnsureSubtreeFor(iter.GetParent(),
                                           iter.GetChildIndex());
    }
    else
        container = mRows.GetRoot();

    if (! container)
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 count;
    OpenSubtreeOf(container, aContainer, &count);

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
nsXULOutlinerBuilder::OpenSubtreeOf(nsOutlinerRows::Subtree* aSubtree,
                                    nsIRDFResource* aContainer,
                                    PRInt32* aDelta)
{
    Instantiation seed;
    seed.AddAssignment(mContainerVar, Value(aContainer));

    InstantiationSet instantiations;
    instantiations.Append(seed);

    // Propogate the assignments through the network
    nsClusterKeySet newkeys;
    mRules.GetRoot()->Propogate(instantiations, &newkeys);

    nsAutoVoidArray open;
    PRInt32 count = 0;

    // Iterate through newly added keys to determine which rules fired
    nsClusterKeySet::ConstIterator last = newkeys.Last();
    for (nsClusterKeySet::ConstIterator key = newkeys.First(); key != last; ++key) {
        const nsTemplateMatchSet* matches;
        mConflictSet.GetMatchesForClusterKey(*key, &matches);

        if (! matches)
            continue;

        nsTemplateMatch* match = 
            matches->FindMatchWithHighestPriority();

        NS_ASSERTION(match != nsnull, "no best match in match set");
        if (! match)
            continue;

        // Remember that this match applied to this row
        mRows.InsertRowAt(match, aSubtree, count);

        // Remember this as the "last" match
        NS_CONST_CAST(nsTemplateMatchSet*, matches)->SetLastMatch(match);

        // If this is open, then remember it so we can recursively add
        // *its* rows to the tree.
        Value val;
        match->GetAssignmentFor(mConflictSet,
                                match->mRule->GetMemberVariable(),
                                &val);

        PRBool isOpen = PR_FALSE;
        IsContainerOpen(VALUE_TO_IRDFRESOURCE(val), &isOpen);
        if (isOpen)
            open.AppendElement((void*) count);

        ++count;
    }

    // Now recursively deal with any open sub-containers that just got
    // inserted
    for (PRInt32 i = 0; i < open.Count(); ++i) {
        PRInt32 index = PRInt32(open[i]);

        nsOutlinerRows::Subtree* child =
            mRows.EnsureSubtreeFor(aSubtree, index);

        nsTemplateMatch* match = (*aSubtree)[index].mMatch;

        Value val;
        match->GetAssignmentFor(mConflictSet, match->mRule->GetMemberVariable(), &val);
        
        PRInt32 delta;
        OpenSubtreeOf(child, VALUE_TO_IRDFRESOURCE(val), &delta);
        count += delta;
    }

    // Sort the container.
    if (mSortVariable && mSortDirection != eDirection_None) {
        NS_QuickSort(mRows.GetRowsFor(aSubtree),
                     aSubtree->Count(),
                     sizeof(nsOutlinerRows::Row),
                     Compare,
                     this);
    }

    *aDelta = count;
    return NS_OK;
}

nsresult
nsXULOutlinerBuilder::CloseContainer(PRInt32 aIndex, nsIRDFResource* aContainer)
{
    NS_ASSERTION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsTemplateMatchSet firings, retractions;
    mConflictSet.Remove(nsOutlinerRowTestNode::Element(aContainer), firings, retractions);

    {
        // Clean up the conflict set
        nsTemplateMatchSet::Iterator last = retractions.Last();
        nsTemplateMatchSet::Iterator iter;

        for (iter = retractions.First(); iter != last; ++iter) {
            Value val;
            iter->GetAssignmentFor(mConflictSet, iter->mRule->GetMemberVariable(), &val);

            RemoveMatchesFor(VALUE_TO_IRDFRESOURCE(val));
        }
    }

    {
        // Update the view
        nsOutlinerRows::iterator iter = mRows[aIndex];

        PRInt32 count = mRows.GetSubtreeSizeFor(iter);
        mRows.RemoveSubtreeFor(iter);

        if (mBoxObject) {
            mBoxObject->InvalidateRow(aIndex);

            if (count)
                mBoxObject->RowCountChanged(aIndex + 1, -count);
        }
    }

    return NS_OK;
}

nsresult
nsXULOutlinerBuilder::RemoveMatchesFor(nsIRDFResource* aContainer)
{
    NS_PRECONDITION(aContainer != nsnull, "null ptr");
    if (! aContainer)
        return NS_ERROR_FAILURE;

    nsTemplateMatchSet firings, retractions;
    mConflictSet.Remove(nsOutlinerRowTestNode::Element(aContainer), firings, retractions);

    nsTemplateMatchSet::Iterator last = retractions.Last();
    nsTemplateMatchSet::Iterator iter;

    for (iter = retractions.First(); iter != last; ++iter) {
        Value val;
        iter->GetAssignmentFor(mConflictSet, iter->mRule->GetMemberVariable(), &val);
        RemoveMatchesFor(VALUE_TO_IRDFRESOURCE(val));
    }

    return NS_OK;
}

nsresult
nsXULOutlinerBuilder::IsContainerOpen(nsIRDFResource* aContainer, PRBool* aResult)
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
nsXULOutlinerBuilder::Compare(const void* aLeft, const void* aRight, void* aClosure)
{
    nsXULOutlinerBuilder* self = NS_STATIC_CAST(nsXULOutlinerBuilder*, aClosure);

    nsOutlinerRows::Row* left = NS_STATIC_CAST(nsOutlinerRows::Row*,
                                               NS_CONST_CAST(void*, aLeft));

    nsOutlinerRows::Row* right = NS_STATIC_CAST(nsOutlinerRows::Row*,
                                                NS_CONST_CAST(void*, aRight));

    return self->CompareMatches(left->mMatch, right->mMatch);
}

PRInt32
nsXULOutlinerBuilder::CompareMatches(nsTemplateMatch* aLeft, nsTemplateMatch* aRight)
{
    PRInt32 result = 0;

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
                    mCollation->CompareRawSortKey(NS_REINTERPRET_CAST(const PRUint8*, lstr),
                                                        nsCRT::strlen(lstr) * sizeof(PRUnichar),
                                                        NS_REINTERPRET_CAST(const PRUint8*, rstr),
                                                        nsCRT::strlen(rstr) * sizeof(PRUnichar),
                                                        &result);
                }
                else
                    result = nsCRT::strcasecmp(lstr, rstr);

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
                LL_SUB(delta, rdate, ldate);

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

                result = rval - lval;

                return result * mSortDirection;
            }
        }
    }

    // Ack! Apples & oranges...
    return 0;
}

nsresult
nsXULOutlinerBuilder::SortSubtree(nsOutlinerRows::Subtree* aSubtree)
{
    NS_QuickSort(mRows.GetRowsFor(aSubtree),
                 aSubtree->Count(),
                 sizeof(nsOutlinerRows::Row),
                 Compare,
                 this);

    for (PRInt32 i = aSubtree->Count() - 1; i >= 0; --i) {
        nsOutlinerRows::Subtree* child = (*aSubtree)[i].mSubtree;
        if (child)
            SortSubtree(child);
    }

    return NS_OK;
}
