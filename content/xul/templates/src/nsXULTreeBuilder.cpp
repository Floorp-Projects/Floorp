/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
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

// For security check
#include "nsIDocument.h"

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

    NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument);

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
      mSortDirection(eDirection_Natural)
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
nsXULOutlinerBuilder::GetResourceAtIndex(PRInt32 aRowIndex, nsIRDFResource** aResult)
{
    if (aRowIndex < 0 || aRowIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    NS_IF_ADDREF(*aResult = GetResourceFor(aRowIndex));
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::GetIndexOfResource(nsIRDFResource* aResource, PRInt32* aResult)
{
    nsOutlinerRows::iterator iter = mRows.Find(mConflictSet, aResource);
    if (iter == mRows.Last())
        *aResult = -1;
    else
        *aResult = iter.GetRowIndex();
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::AddObserver(nsIXULOutlinerBuilderObserver* aObserver)
{
    nsresult rv;  
    if (!mObservers) {
        rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
        if (NS_FAILED(rv)) return rv;
    }

    return mObservers->AppendElement(aObserver);
}

NS_IMETHODIMP
nsXULOutlinerBuilder::RemoveObserver(nsIXULOutlinerBuilderObserver* aObserver)
{
    return mObservers ? mObservers->RemoveElement(aObserver) : NS_ERROR_FAILURE;
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
        row->GetAttr(kNameSpaceID_None, nsXULAtoms::properties, raw);

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
        cell->GetAttr(kNameSpaceID_None, nsXULAtoms::properties, raw);

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

    nsOutlinerRows::iterator iter = mRows[aIndex];

    if (iter->mContainerType == nsOutlinerRows::eContainerType_Unknown) {
        PRBool isContainer;
        CheckContainer(GetResourceFor(aIndex), &isContainer, nsnull);

        iter->mContainerType = isContainer
            ? nsOutlinerRows::eContainerType_Container
            : nsOutlinerRows::eContainerType_Noncontainer;
    }

    *aResult = (iter->mContainerType == nsOutlinerRows::eContainerType_Container);
    return NS_OK;
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

    nsOutlinerRows::iterator iter = mRows[aIndex];
    NS_ASSERTION(iter->mContainerType == nsOutlinerRows::eContainerType_Container,
                 "asking for empty state on non-container");

    if (iter->mContainerState == nsOutlinerRows::eContainerState_Unknown) {
        PRBool isEmpty;
        CheckContainer(GetResourceFor(aIndex), nsnull, &isEmpty);

        iter->mContainerState = isEmpty
            ? nsOutlinerRows::eContainerState_Empty
            : nsOutlinerRows::eContainerState_Nonempty;
    }

    *aResult = (iter->mContainerState == nsOutlinerRows::eContainerState_Empty);
    return NS_OK;
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
        cell->GetAttr(kNameSpaceID_None, nsXULAtoms::label, raw);

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

        nsCOMPtr<nsIDocument> doc;
        mRoot->GetDocument(*getter_AddRefs(doc));
        NS_ASSERTION(doc, "element has no document");
        if (!doc)
            return NS_ERROR_UNEXPECTED;

        // Grab the doc's principal...
        nsCOMPtr<nsIPrincipal> docPrincipal;
        nsresult rv = doc->GetPrincipal(getter_AddRefs(docPrincipal));
        if (NS_FAILED(rv)) 
            return rv;

        PRBool isTrusted = PR_FALSE;
        rv = IsSystemPrincipal(docPrincipal.get(), &isTrusted);
        if (NS_SUCCEEDED(rv) && isTrusted) {
            // Get the datasource we intend to use to remember open state.
            nsAutoString datasourceStr;
            mRoot->GetAttr(kNameSpaceID_None, nsXULAtoms::statedatasource, datasourceStr);

            // since we are trusted, use the user specified datasource
            // if non specified, use localstore, which gives us
            // persistance across sessions
            if (datasourceStr.Length()) {
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

        NS_ASSERTION(mPersistStateStore, "failed to get a persistant state store");
        if (! mPersistStateStore)
            return NS_ERROR_FAILURE;

        Rebuild();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::ToggleOpenState(PRInt32 aIndex)
{
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnToggleOpenState(aIndex);
        }
    }
    
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
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnCycleHeader(aColID, aElement);
        }
    }

    nsCOMPtr<nsIContent> header = do_QueryInterface(aElement);
    if (! header)
        return NS_ERROR_FAILURE;

    nsAutoString sort;
    header->GetAttr(kNameSpaceID_None, nsXULAtoms::sort, sort);

    if (sort.Length()) {
        // Grab the new sort variable
        mSortVariable = mRules.LookupSymbol(sort.get());

        // Cycle the sort direction
        nsAutoString dir;
        header->GetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, dir);

        if (dir == NS_LITERAL_STRING("ascending")) {
            dir = NS_LITERAL_STRING("descending");
            mSortDirection = eDirection_Descending;
        }
        else if (dir == NS_LITERAL_STRING("descending")) {
            dir = NS_LITERAL_STRING("natural");
            mSortDirection = eDirection_Natural;
        }
        else {
            dir = NS_LITERAL_STRING("ascending");
            mSortDirection = eDirection_Ascending;
        }

        // Sort it
        SortSubtree(mRows.GetRoot());
        mRows.InvalidateCachedRow();
        mBoxObject->Invalidate();

        header->SetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, dir, PR_FALSE);

        // Unset sort attribute(s) on the other columns
        nsCOMPtr<nsIContent> parentContent;
        header->GetParent(*getter_AddRefs(parentContent));
        if (parentContent) {
            nsCOMPtr<nsIAtom> parentTag;
            parentContent->GetTag(*getter_AddRefs(parentTag));
            if (parentTag.get() == nsXULAtoms::outliner) {
                PRInt32 numChildren;
                parentContent->ChildCount(numChildren);
                for (int i = 0; i < numChildren; ++i) {
                    nsCOMPtr<nsIContent> childContent;
                    nsCOMPtr<nsIAtom> childTag;
                    parentContent->ChildAt(i, *getter_AddRefs(childContent));
                    if (childContent) {
                        childContent->GetTag(*getter_AddRefs(childTag));
                        if (childTag.get() == nsXULAtoms::outlinercol && childContent != header) {
                            childContent->UnsetAttr(kNameSpaceID_None,
                                                    nsXULAtoms::sortDirection, PR_FALSE);
                        }
                    }
                }
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::SelectionChanged()
{
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnSelectionChanged();
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::CycleCell(PRInt32 row, const PRUnichar* colID)
{
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnCycleCell(row, colID);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::IsEditable(PRInt32 row, const PRUnichar* colID, PRBool* _retval)
{
    *_retval = PR_FALSE;
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer) {
                observer->IsEditable(row, colID, _retval);
                if (*_retval)
                    // No need to keep asking, show a textfield as at least one client will handle it. 
                    break;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::SetCellText(PRInt32 row, const PRUnichar* colID, const PRUnichar* value)
{
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer) {
                // If the current observer supports a name change operation, go ahead and invoke it. 
                PRBool isEditable = PR_FALSE;
                observer->IsEditable(row, colID, &isEditable);
                if (isEditable)
                    observer->OnSetCellText(row, colID, value);
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::PerformAction(const PRUnichar* action)
{
    if (mObservers) {  
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnPerformAction(action);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::PerformActionOnRow(const PRUnichar* action, PRInt32 row)
{
    if (mObservers) {  
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnPerformActionOnRow(action, row);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::PerformActionOnCell(const PRUnichar* action, PRInt32 row, const PRUnichar* colID)
{
    if (mObservers) {  
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer)
                observer->OnPerformActionOnCell(action, row, colID);
        }
    }

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
        const char* s = "(null)";
        if (root)
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

NS_IMETHODIMP
nsXULOutlinerBuilder::DocumentWillBeDestroyed(nsIDocument* aDocument)
{
    if (mObservers)
        mObservers->Clear();

    return nsXULTemplateBuilder::DocumentWillBeDestroyed(aDocument);
}

 
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
            Value val;
            NS_CONST_CAST(nsTemplateMatch*, aOldMatch)->GetAssignmentFor(mConflictSet, mContainerVar, &val);

            nsIRDFResource* container = VALUE_TO_IRDFRESOURCE(val);
            RemoveMatchesFor(container, aMember);

            // Remove the rows from the view
            PRInt32 row = iter.GetRowIndex();
            PRInt32 delta = mRows.GetSubtreeSizeFor(iter);
            mRows.RemoveRowAt(iter);

            // XXX Could potentially invalidate the iterator's
            // mContainer[Type|State] caching here, but it'll work
            // itself out.

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

            if (mPersistStateStore)
                mPersistStateStore->HasAssertion(container,
                                                 nsXULContentUtils::NC_open,
                                                 nsXULContentUtils::true_,
                                                 PR_TRUE,
                                                 &open);

            if (open) {
                parent = mRows.EnsureSubtreeFor(iter);
            }
            else if ((iter->mContainerType != nsOutlinerRows::eContainerType_Container) ||
                     (iter->mContainerState != nsOutlinerRows::eContainerState_Nonempty)) {
                // The container is closed, but we know something has
                // just been inserted into it.
                iter->mContainerType  = nsOutlinerRows::eContainerType_Container;
                iter->mContainerState = nsOutlinerRows::eContainerState_Nonempty;
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
                        right = index - 1;
                    else
                        break;
                }
            }

            mRows.InsertRowAt(aNewMatch, parent, index);
            mBoxObject->RowCountChanged(row + index + 1, +1);
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

        nsOutlinerRows::iterator iter =
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
        child->GetAttr(kNameSpaceID_None, nsXULAtoms::sort, sort);
        if (! sort.Length())
            continue;

        PRInt32 var = mRules.LookupSymbol(sort.get(), PR_TRUE);
        aVariables.Add(var);

        nsAutoString active;
        child->GetAttr(kNameSpaceID_None, nsXULAtoms::sortActive, active);
        if (active == NS_LITERAL_STRING("true")) {
            nsAutoString dir;
            child->GetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, dir);

            if (dir == NS_LITERAL_STRING("none"))
                mSortDirection = eDirection_Natural;
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
    aCondition->GetAttr(kNameSpaceID_None, nsXULAtoms::uri, uri);

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
            child->GetAttr(kNameSpaceID_None, nsXULAtoms::ref, ref);

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

    nsAString::const_iterator end;
    aString.EndReading(end);

    nsAString::const_iterator iter;
    aString.BeginReading(iter);

    do {
        // Skip whitespace
        while (iter != end && nsCRT::IsAsciiSpace(*iter))
            ++iter;

        // If only whitespace, we're done
        if (iter == end)
            break;

        // Note the first non-whitespace character
        nsAString::const_iterator first = iter;

        // Advance to the next whitespace character
        while (iter != end && ! nsCRT::IsAsciiSpace(*iter))
            ++iter;

        // XXX this would be nonsensical
        NS_ASSERTION(iter != first, "eh? something's wrong here");
        if (iter == first)
            break;

        nsCOMPtr<nsIAtom> atom = dont_AddRef(NS_NewAtom(Substring(first, iter)));
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

    // Propogate the assignments through the network
    nsClusterKeySet newkeys;
    mRules.GetRoot()->Propogate(instantiations, &newkeys);

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

        // Remember that this match applied to this row
        mRows.InsertRowAt(match, aSubtree, count);

        // Remember this as the "last" match
        matches->mLastMatch = match;

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
        PRInt32 index = NS_PTR_TO_INT32(open[i]);

        nsOutlinerRows::Subtree* child =
            mRows.EnsureSubtreeFor(aSubtree, index);

        nsTemplateMatch* match = (*aSubtree)[index].mMatch;

        Value val;
        match->GetAssignmentFor(mConflictSet, match->mRule->GetMemberVariable(), &val);
        
        PRInt32 delta;
        OpenSubtreeOf(child, VALUE_TO_IRDFRESOURCE(val), &delta);
        count += delta;
    }

#ifdef PR_LOGGING
    --gNest;
#endif

    // Sort the container.
    if (mSortVariable) {
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
    mConflictSet.Remove(nsOutlinerRowTestNode::Element(aContainer), firings, retractions);

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
nsXULOutlinerBuilder::RemoveMatchesFor(nsIRDFResource* aContainer, nsIRDFResource* aMember)
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
    mConflictSet.Remove(nsOutlinerRowTestNode::Element(aMember), firings, retractions);

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

    if (mSortDirection == eDirection_Natural) {
        // If the sort order is ``natural'', then see if the container
        // is an RDF sequence. If so, we'll try to use the ordinal
        // properties to determine order.
        //
        // XXX the problem with this is, it doesn't always get the
        // *real* container; e.g.,
        //
        //  <outlinerrow uri="?uri" />
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


/* boolean canDropOn (in long index); */
NS_IMETHODIMP
nsXULOutlinerBuilder::CanDropOn(PRInt32 index, PRBool *_retval)
{
    *_retval = PR_FALSE;
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer) {
                observer->CanDropOn(index, _retval);
                if (*_retval)
                    break;
            }
        }
    }

    return NS_OK;
}

/* boolean canDropBeforeAfter (in long index, in boolean before); */
NS_IMETHODIMP
nsXULOutlinerBuilder::CanDropBeforeAfter(PRInt32 index, PRBool before, PRBool *_retval)
{
    *_retval = PR_FALSE;
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer) {
                observer->CanDropBeforeAfter(index, before, _retval);
                if (*_retval)
                    break;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::Drop(PRInt32 row, PRInt32 orient)
{
    if (mObservers) {
        PRUint32 count;
        mObservers->Count(&count);
        for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIXULOutlinerBuilderObserver> observer;
            mObservers->QueryElementAt(i, NS_GET_IID(nsIXULOutlinerBuilderObserver), getter_AddRefs(observer));
            if (observer) {
                PRBool canDropOn = PR_FALSE;
                observer->CanDropOn(row, &canDropOn);
                if (canDropOn)
                    observer->OnDrop(row, orient);
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULOutlinerBuilder::IsSorted(PRBool *_retval)
{
  *_retval = mSortVariable;
  return NS_OK;
}

