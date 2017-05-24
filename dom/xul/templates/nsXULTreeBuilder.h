/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULTreeBuilder_h__
#define nsXULTreeBuilder_h__

#include "nsCOMPtr.h"
#include "nsITreeView.h"
#include "nsTreeRows.h"
#include "nsXULTemplateBuilder.h"

class nsIContent;
class nsIRDFResource;
class nsITreeSelection;
class nsIXULStore;
class nsIXULTemplateResult;
class nsTreeColumn;

namespace mozilla {
namespace dom {

class DataTransfer;
class TreeBoxObject;
class XULTreeBuilderObserver;

} // namespace dom
} // namespace mozilla

/**
 * A XUL template builder that serves as an tree view, allowing
 * (pretty much) arbitrary RDF to be presented in an tree.
 */
class nsXULTreeBuilder : public nsXULTemplateBuilder,
                         public nsIXULTreeBuilder,
                         public nsINativeTreeView
{
public:
    explicit nsXULTreeBuilder(Element* aElement);

    // nsISupports
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXULTreeBuilder,
                                             nsXULTemplateBuilder)

    virtual JSObject* WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) override;

    already_AddRefed<nsIRDFResource>
      GetResourceAtIndex(int32_t aIndex, mozilla::ErrorResult& aError);
    int32_t GetIndexOfResource(nsIRDFResource* aResource,
                               mozilla::ErrorResult& aError);
    void AddObserver(mozilla::dom::XULTreeBuilderObserver& aObserver);
    void RemoveObserver(mozilla::dom::XULTreeBuilderObserver& aObserver);
    void Sort(Element&);

    int32_t RowCount()
    {
        return mRows.Count();
    }
    nsITreeSelection* GetSelection()
    {
        return mSelection;
    }
    void SetSelection(nsITreeSelection* aSelection,
                      mozilla::ErrorResult& aError);
    void GetRowProperties(int32_t aRow, nsAString& aProperties,
                          mozilla::ErrorResult& aError);
    void GetCellProperties(int32_t aRow, nsTreeColumn& aColumn,
                           nsAString& aProperties, mozilla::ErrorResult& aError);
    void GetColumnProperties(nsTreeColumn& aColumn, nsAString& aProperties)
    {
    }
    bool IsContainer(int32_t aRow, mozilla::ErrorResult& aError);
    bool IsContainerOpen(int32_t aRow, mozilla::ErrorResult& aError);
    bool IsContainerEmpty(int32_t aRow, mozilla::ErrorResult& aError);
    bool IsSeparator(int32_t aRow, mozilla::ErrorResult& aError);
    bool IsSorted()
    {
        return mSortVariable != nullptr;
    }
    bool CanDrop(int32_t aRow, int32_t aOrientation,
                 mozilla::dom::DataTransfer* aDataTransfer,
                 mozilla::ErrorResult& aError);
    void Drop(int32_t aRow, int32_t aOrientation,
              mozilla::dom::DataTransfer* aDataTransfer,
              mozilla::ErrorResult& aError);
    int32_t GetParentIndex(int32_t aRow, mozilla::ErrorResult& aError);
    bool HasNextSibling(int32_t aRow, int32_t aAfterIndex,
                        mozilla::ErrorResult& aError);
    int32_t GetLevel(int32_t aRow, mozilla::ErrorResult& aError);
    void GetImageSrc(int32_t aRow, nsTreeColumn& aColumn, nsAString& aSrc,
                     mozilla::ErrorResult& aError);
    int32_t GetProgressMode(int32_t aRow, nsTreeColumn& aColumn,
                            mozilla::ErrorResult& aError);
    void GetCellValue(int32_t aRow, nsTreeColumn& aColumn, nsAString& aValue,
                      mozilla::ErrorResult& aError);
    void GetCellText(int32_t aRow, nsTreeColumn& aColumn, nsAString& aText,
                     mozilla::ErrorResult& aError);
    void SetTree(mozilla::dom::TreeBoxObject* aTree,
                 mozilla::ErrorResult& aError);
    void ToggleOpenState(int32_t aRow, mozilla::ErrorResult& aError);
    void CycleHeader(nsTreeColumn& aColumn, mozilla::ErrorResult& aError);
    // XPCOM SelectionChanged() is OK
    void CycleCell(int32_t aRow, nsTreeColumn& aColumn);
    bool IsEditable(int32_t aRow, nsTreeColumn& aColumn,
                    mozilla::ErrorResult& aError);
    bool IsSelectable(int32_t aRow, nsTreeColumn& aColumn,
                      mozilla::ErrorResult& aError);
    void SetCellValue(int32_t aRow, nsTreeColumn& aColumn,
                      const nsAString& aValue, mozilla::ErrorResult& aError)
    {
    }
    void SetCellText(int32_t aRow, nsTreeColumn& aColumn,
                      const nsAString& aText, mozilla::ErrorResult& aError)
    {
    }
    void PerformAction(const nsAString& aAction);
    void PerformActionOnRow(const nsAString& aAction, int32_t aRow);
    void PerformActionOnCell(const nsAString& aAction, int32_t aRow,
                             nsTreeColumn& aColumn);

    using nsIXULTemplateBuilder::HasGeneratedContent;
    virtual bool HasGeneratedContent(nsIRDFResource* aResource,
                                     const nsAString& aTag,
                                     mozilla::ErrorResult& aError) override;

    // nsIXULTreeBuilder
    NS_DECL_NSIXULTREEBUILDER

    // nsITreeView
    NS_DECL_NSITREEVIEW
    // nsINativeTreeView: Untrusted code can use us
    NS_IMETHOD EnsureNative() override { return NS_OK; }

    // nsIMutationObserver
    NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

protected:
    friend struct ResultComparator;

    ~nsXULTreeBuilder();

    /**
     * Uninitialize the template builder
     */
    virtual void Uninit(bool aIsFinal) override;

    /**
     * Get sort variables from the active <treecol>
     */
    nsresult
    EnsureSortVariables();

    virtual nsresult
    RebuildAll() override;

    /**
     * Given a row, use the row's match to figure out the appropriate
     * <treerow> in the rule's <action>.
     */
    nsresult
    GetTemplateActionRowFor(int32_t aRow, nsIContent** aResult);

    /**
     * Given a row and a column ID, use the row's match to figure out
     * the appropriate <treecell> in the rule's <action>.
     */
    nsIContent*
    GetTemplateActionCellFor(int32_t aRow, nsTreeColumn& aCol);

    /**
     * Return the resource corresponding to a row in the tree.
     */
    nsresult
    GetResourceFor(int32_t aRow, nsIRDFResource** aResource);

    /**
     * Open a container row, inserting the container's children into
     * the view.
     */
    nsresult
    OpenContainer(int32_t aIndex, nsIXULTemplateResult* aResult);

    /**
     * Helper for OpenContainer, recursively open subtrees, remembering
     * persisted ``open'' state
     */
    nsresult
    OpenSubtreeOf(nsTreeRows::Subtree* aSubtree,
                  int32_t aIndex,
                  nsIXULTemplateResult *aResult,
                  int32_t* aDelta);

    nsresult
    OpenSubtreeForQuerySet(nsTreeRows::Subtree* aSubtree,
                           int32_t aIndex,
                           nsIXULTemplateResult *aResult,
                           nsTemplateQuerySet* aQuerySet,
                           int32_t* aDelta,
                           nsTArray<int32_t>& open);

    /**
     * Close a container row, removing the container's childrem from
     * the view.
     */
    nsresult
    CloseContainer(int32_t aIndex);

    /**
     * Remove the matches for the rows in a subtree
     */
    nsresult
    RemoveMatchesFor(nsTreeRows::Subtree& subtree);

    /**
     * Helper method that determines if the specified container is open.
     */
    bool
    IsContainerOpen(nsIXULTemplateResult* aResource);

    /**
     * A sorting callback for NS_QuickSort().
     */
    static int
    Compare(const void* aLeft, const void* aRight, void* aClosure);

    /**
     * The real sort routine
     */
    int32_t
    CompareResults(nsIXULTemplateResult* aLeft, nsIXULTemplateResult* aRight);

    /**
     * Sort the specified subtree, and recursively sort any subtrees
     * beneath it.
     */
    nsresult
    SortSubtree(nsTreeRows::Subtree* aSubtree);

    // GetInsertionLocations, ReplaceMatch and SynchronizeResult are inherited
    // from nsXULTemplateBuilder

    /**
     * Return true if the result can be inserted into the template as a new
     * row.
     */
    bool
    GetInsertionLocations(nsIXULTemplateResult* aResult,
                          nsCOMArray<nsIContent>** aLocations) override;

    /**
     * Implement result replacement
     */
    virtual nsresult
    ReplaceMatch(nsIXULTemplateResult* aOldResult,
                 nsTemplateMatch* aNewMatch,
                 nsTemplateRule* aNewMatchRule,
                 void* aContext) override;

    /**
     * Implement match synchronization
     */
    virtual nsresult
    SynchronizeResult(nsIXULTemplateResult* aResult) override;

    bool IsValidRowIndex(int32_t aRowIndex);

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
    uint32_t mSortHints;

    /** 
     * The builder observers.
     */
    nsTArray<nsCOMPtr<nsIXULTreeBuilderObserver>> mObservers;

    /*
     * XUL store for holding open container state
     */
    nsCOMPtr<nsIXULStore> mLocalStore;
};

#endif // nsXULTreeBuilder_h__
