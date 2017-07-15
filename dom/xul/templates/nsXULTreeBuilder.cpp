/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULTreeBuilder.h"
#include "nscore.h"
#include "nsError.h"
#include "nsIContent.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsIDOMElement.h"
#include "nsIBoxObject.h"
#include "nsITreeBoxObject.h"
#include "nsITreeSelection.h"
#include "nsITreeColumns.h"
#include "nsTreeUtils.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsQuickSort.h"
#include "nsTemplateRule.h"
#include "nsTemplateMatch.h"
#include "nsTreeColumns.h"
#include "nsGkAtoms.h"
#include "nsXULContentUtils.h"
#include "nsIXULSortService.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"
#include "nsNameSpaceManager.h"
#include "nsWhitespaceTokenizer.h"
#include "nsTreeContentView.h"
#include "nsIXULStore.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/TreeBoxObject.h"
#include "mozilla/dom/XULTemplateBuilderBinding.h"

// For security check
#include "nsIDocument.h"

NS_IMPL_ADDREF_INHERITED(nsXULTreeBuilder, nsXULTemplateBuilder)
NS_IMPL_RELEASE_INHERITED(nsXULTreeBuilder, nsXULTemplateBuilder)

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsXULTreeBuilder, nsXULTemplateBuilder,
                                   mBoxObject,
                                   mSelection,
                                   mPersistStateStore,
                                   mLocalStore,
                                   mObservers)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXULTreeBuilder)
    NS_INTERFACE_MAP_ENTRY(nsIXULTreeBuilder)
    NS_INTERFACE_MAP_ENTRY(nsITreeView)
NS_INTERFACE_MAP_END_INHERITING(nsXULTemplateBuilder)


nsXULTreeBuilder::nsXULTreeBuilder(Element* aElement)
    : nsXULTemplateBuilder(aElement),
      mSortDirection(eDirection_Natural), mSortHints(0)
{
}

nsXULTreeBuilder::~nsXULTreeBuilder()
{
}

JSObject*
nsXULTreeBuilder::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
    return XULTreeBuilderBinding::Wrap(aCx, this, aGivenProto);
}

void
nsXULTreeBuilder::Uninit(bool aIsFinal)
{
    int32_t count = mRows.Count();
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

already_AddRefed<nsIRDFResource>
nsXULTreeBuilder::GetResourceAtIndex(int32_t aRowIndex, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRowIndex)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return nullptr;
    }

    nsCOMPtr<nsIRDFResource> result;
    aError = GetResourceFor(aRowIndex, getter_AddRefs(result));
    return result.forget();
}

NS_IMETHODIMP
nsXULTreeBuilder::GetResourceAtIndex(int32_t aRowIndex, nsIRDFResource** aResult)
{
    ErrorResult rv;
    *aResult = GetResourceAtIndex(aRowIndex, rv).take();
    return rv.StealNSResult();
}

int32_t
nsXULTreeBuilder::GetIndexOfResource(nsIRDFResource* aResource,
                                     ErrorResult& aError)
{
    nsTreeRows::iterator iter = mRows.FindByResource(aResource);
    return iter == mRows.Last() ? -1 : iter.GetRowIndex();
}

NS_IMETHODIMP
nsXULTreeBuilder::GetIndexOfResource(nsIRDFResource* aResource, int32_t* aResult)
{
    NS_ENSURE_ARG_POINTER(aResource);

    ErrorResult rv;
    *aResult = GetIndexOfResource(aResource, rv);
    return rv.StealNSResult();
}

void
nsXULTreeBuilder::AddObserver(XULTreeBuilderObserver& aObserver)
{
    CallbackObjectHolder<XULTreeBuilderObserver, nsIXULTreeBuilderObserver>
        holder(&aObserver);
    mObservers.AppendElement(holder.ToXPCOMCallback());
}

NS_IMETHODIMP
nsXULTreeBuilder::AddObserver(nsIXULTreeBuilderObserver* aObserver)
{
    mObservers.AppendElement(aObserver);
    return NS_OK;
}

void
nsXULTreeBuilder::RemoveObserver(XULTreeBuilderObserver& aObserver)
{
    CallbackObjectHolder<XULTreeBuilderObserver, nsIXULTreeBuilderObserver>
        holder(&aObserver);
    nsCOMPtr<nsIXULTreeBuilderObserver> observer(holder.ToXPCOMCallback());
    mObservers.RemoveElement(observer);
}

NS_IMETHODIMP
nsXULTreeBuilder::RemoveObserver(nsIXULTreeBuilderObserver* aObserver)
{
    mObservers.RemoveElement(aObserver);
    return NS_OK;
}

void
nsXULTreeBuilder::Sort(Element& aElement)
{
    if (aElement.AttrValueIs(kNameSpaceID_None, nsGkAtoms::sortLocked,
                             nsGkAtoms::_true, eCaseMatters)) {
        return;
    }

    nsAutoString sort;
    aElement.GetAttr(kNameSpaceID_None, nsGkAtoms::sort, sort);

    if (sort.IsEmpty()) {
        return;
    }

    // Grab the new sort variable
    mSortVariable = NS_Atomize(sort);

    nsAutoString hints;
    aElement.GetAttr(kNameSpaceID_None, nsGkAtoms::sorthints, hints);

    bool hasNaturalState = true;
    nsWhitespaceTokenizer tokenizer(hints);
    while (tokenizer.hasMoreTokens()) {
      const nsDependentSubstring& token(tokenizer.nextToken());
      if (token.EqualsLiteral("comparecase"))
        mSortHints |= nsIXULSortService::SORT_COMPARECASE;
      else if (token.EqualsLiteral("integer"))
        mSortHints |= nsIXULSortService::SORT_INTEGER;
      else if (token.EqualsLiteral("twostate"))
        hasNaturalState = false;
    }

    // Cycle the sort direction
    nsAutoString dir;
    aElement.GetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection, dir);

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

    nsTreeUtils::UpdateSortIndicators(&aElement, dir);
}

NS_IMETHODIMP
nsXULTreeBuilder::Sort(nsIDOMElement* aElement)
{
    nsCOMPtr<Element> header = do_QueryInterface(aElement);
    if (!header) {
        return NS_ERROR_FAILURE;
    }
    Sort(*header);
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsITreeView methods
//

NS_IMETHODIMP
nsXULTreeBuilder::GetRowCount(int32_t* aRowCount)
{
    *aRowCount = RowCount();
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetSelection(nsITreeSelection** aSelection)
{
    NS_IF_ADDREF(*aSelection = GetSelection());
    return NS_OK;
}

void
nsXULTreeBuilder::SetSelection(nsITreeSelection* aSelection,
                               ErrorResult& aError)
{
    if (aSelection && !nsTreeContentView::CanTrustTreeSelection(aSelection)) {
        aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return;
    }

    mSelection = aSelection;
}

NS_IMETHODIMP
nsXULTreeBuilder::SetSelection(nsITreeSelection* aSelection)
{
    ErrorResult rv;
    SetSelection(aSelection, rv);
    return rv.StealNSResult();
}

void
nsXULTreeBuilder::GetRowProperties(int32_t aRow, nsAString& aProperties,
                                   ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return;
    }

    nsCOMPtr<nsIContent> row;
    GetTemplateActionRowFor(aRow, getter_AddRefs(row));
    if (row) {
        nsAutoString raw;
        row->GetAttr(kNameSpaceID_None, nsGkAtoms::properties, raw);

        if (!raw.IsEmpty()) {
            SubstituteText(mRows[aRow]->mMatch->mResult, raw, aProperties);
        }
    }
}

NS_IMETHODIMP
nsXULTreeBuilder::GetRowProperties(int32_t aIndex, nsAString& aProps)
{
    ErrorResult rv;
    GetRowProperties(aIndex, aProps, rv);
    return rv.StealNSResult();
}

void
nsXULTreeBuilder::GetCellProperties(int32_t aRow, nsTreeColumn& aColumn,
                                    nsAString& aProperties, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return;
    }

    nsIContent* cell = GetTemplateActionCellFor(aRow, aColumn);
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::properties, raw);

        if (!raw.IsEmpty()) {
            SubstituteText(mRows[aRow]->mMatch->mResult, raw, aProperties);
        }
    }
}

NS_IMETHODIMP
nsXULTreeBuilder::GetCellProperties(int32_t aRow, nsITreeColumn* aCol,
                                    nsAString& aProps)
{
    RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
    NS_ENSURE_ARG(col);

    ErrorResult rv;
    GetCellProperties(aRow, *col, aProps, rv);
    return rv.StealNSResult();
}

NS_IMETHODIMP
nsXULTreeBuilder::GetColumnProperties(nsITreeColumn* aCol, nsAString& aProps)
{
    NS_ENSURE_ARG_POINTER(aCol);
    // XXX sortactive fu
    return NS_OK;
}

bool
nsXULTreeBuilder::IsContainer(int32_t aRow, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return false;
    }

    nsTreeRows::iterator iter = mRows[aRow];

    bool isContainer;
    iter->mMatch->mResult->GetIsContainer(&isContainer);

    iter->mContainerType = isContainer
        ? nsTreeRows::eContainerType_Container
        : nsTreeRows::eContainerType_Noncontainer;

    return iter->mContainerType == nsTreeRows::eContainerType_Container;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsContainer(int32_t aIndex, bool* aResult)
{
    ErrorResult rv;
    *aResult = IsContainer(aIndex, rv);
    return rv.StealNSResult();
}

bool
nsXULTreeBuilder::IsContainerOpen(int32_t aRow, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return false;
    }

    nsTreeRows::iterator iter = mRows[aRow];

    if (iter->mContainerState == nsTreeRows::eContainerState_Unknown) {
        bool isOpen = IsContainerOpen(iter->mMatch->mResult);

        iter->mContainerState = isOpen
            ? nsTreeRows::eContainerState_Open
            : nsTreeRows::eContainerState_Closed;
    }

    return iter->mContainerState == nsTreeRows::eContainerState_Open;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsContainerOpen(int32_t aIndex, bool* aOpen)
{
    ErrorResult rv;
    *aOpen = IsContainerOpen(aIndex, rv);
    return rv.StealNSResult();
}

bool
nsXULTreeBuilder::IsContainerEmpty(int32_t aRow, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return false;
    }

    nsTreeRows::iterator iter = mRows[aRow];
    NS_ASSERTION(iter->mContainerType == nsTreeRows::eContainerType_Container,
                 "asking for empty state on non-container");

    // if recursion is disabled, pretend that the container is empty. This
    // ensures that folders are still displayed as such, yet won't display
    // their children
    if ((mFlags & eDontRecurse) && (iter->mMatch->mResult != mRootResult)) {
        return true;
    }

    if (iter->mContainerFill == nsTreeRows::eContainerFill_Unknown) {
        bool isEmpty;
        iter->mMatch->mResult->GetIsEmpty(&isEmpty);

        iter->mContainerFill = isEmpty
            ? nsTreeRows::eContainerFill_Empty
            : nsTreeRows::eContainerFill_Nonempty;
    }

    return iter->mContainerFill == nsTreeRows::eContainerFill_Empty;
}

NS_IMETHODIMP
nsXULTreeBuilder::IsContainerEmpty(int32_t aIndex, bool* aResult)
{
    ErrorResult rv;
    *aResult = IsContainerEmpty(aIndex, rv);
    return rv.StealNSResult();
}

bool
nsXULTreeBuilder::IsSeparator(int32_t aRow, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return false;
    }

    nsAutoString type;
    nsTreeRows::Row& row = *(mRows[aRow]);
    row.mMatch->mResult->GetType(type);

    return type.EqualsLiteral("separator");
}

NS_IMETHODIMP
nsXULTreeBuilder::IsSeparator(int32_t aIndex, bool* aResult)
{
    ErrorResult rv;
    *aResult = IsSeparator(aIndex, rv);
    return rv.StealNSResult();
}

int32_t
nsXULTreeBuilder::GetParentIndex(int32_t aRow, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return -1;
    }

    // Construct a path to the row
    nsTreeRows::iterator iter = mRows[aRow];

    // The parent of the row will be at the top of the path
    nsTreeRows::Subtree* parent = iter.GetParent();

    // Now walk through our previous siblings, subtracting off each
    // one's subtree size
    int32_t index = iter.GetChildIndex();
    while (--index >= 0)
        aRow -= mRows.GetSubtreeSizeFor(parent, index) + 1;

    // Now the parent's index will be the first row's index, less one.
    return aRow - 1;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetParentIndex(int32_t aRowIndex, int32_t* aResult)
{
    ErrorResult rv;
    *aResult = GetParentIndex(aRowIndex, rv);
    return rv.StealNSResult();
}

bool
nsXULTreeBuilder::HasNextSibling(int32_t aRow, int32_t aAfterIndex,
                                 ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return false;
    }

    // Construct a path to the row
    nsTreeRows::iterator iter = mRows[aRow];

    // The parent of the row will be at the top of the path
    nsTreeRows::Subtree* parent = iter.GetParent();

    // We have a next sibling if the child is not the last in the
    // subtree.
    return iter.GetChildIndex() != parent->Count() - 1;
}

NS_IMETHODIMP
nsXULTreeBuilder::HasNextSibling(int32_t aRowIndex, int32_t aAfterIndex, bool* aResult)
{
    ErrorResult rv;
    *aResult = HasNextSibling(aRowIndex, aAfterIndex, rv);
    return rv.StealNSResult();
}

int32_t
nsXULTreeBuilder::GetLevel(int32_t aRow, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return -1;
    }

    // Construct a path to the row; the ``level'' is the path length
    // less one.
    nsTreeRows::iterator iter = mRows[aRow];
    return iter.GetDepth() - 1;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetLevel(int32_t aRowIndex, int32_t* aResult)
{
    ErrorResult rv;
    *aResult = GetLevel(aRowIndex, rv);
    return rv.StealNSResult();
}

void
nsXULTreeBuilder::GetImageSrc(int32_t aRow, nsTreeColumn& aColumn,
                              nsAString& aSrc, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return;
    }

    // Find the <cell> that corresponds to the column we want.
    nsIContent* cell = GetTemplateActionCellFor(aRow, aColumn);
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::src, raw);

        SubstituteText(mRows[aRow]->mMatch->mResult, raw, aSrc);
    } else {
        aSrc.Truncate();
    }
}

NS_IMETHODIMP
nsXULTreeBuilder::GetImageSrc(int32_t aRow, nsITreeColumn* aCol, nsAString& aResult)
{
    RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
    NS_ENSURE_ARG(col);

    ErrorResult rv;
    GetImageSrc(aRow, *col, aResult, rv);
    return rv.StealNSResult();
}

int32_t
nsXULTreeBuilder::GetProgressMode(int32_t aRow, nsTreeColumn& aColumn,
                                  ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return -1;
    }

    // Find the <cell> that corresponds to the column we want.
    nsIContent* cell = GetTemplateActionCellFor(aRow, aColumn);
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::mode, raw);

        nsAutoString mode;
        SubstituteText(mRows[aRow]->mMatch->mResult, raw, mode);

        if (mode.EqualsLiteral("normal")) {
            return nsITreeView::PROGRESS_NORMAL;
        }

        if (mode.EqualsLiteral("undetermined")) {
            return nsITreeView::PROGRESS_UNDETERMINED;
        }
    }

    return nsITreeView::PROGRESS_NONE;
}

NS_IMETHODIMP
nsXULTreeBuilder::GetProgressMode(int32_t aRow, nsITreeColumn* aCol, int32_t* aResult)
{
    RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
    NS_ENSURE_ARG(col);

    ErrorResult rv;
    *aResult = GetProgressMode(aRow, *col, rv);
    return rv.StealNSResult();
}

void
nsXULTreeBuilder::GetCellValue(int32_t aRow, nsTreeColumn& aColumn,
                               nsAString& aValue, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return;
    }

    // Find the <cell> that corresponds to the column we want.
    nsIContent* cell = GetTemplateActionCellFor(aRow, aColumn);
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::value, raw);

        SubstituteText(mRows[aRow]->mMatch->mResult, raw, aValue);
    } else {
        aValue.Truncate();
    }
}

NS_IMETHODIMP
nsXULTreeBuilder::GetCellValue(int32_t aRow, nsITreeColumn* aCol, nsAString& aResult)
{
    RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
    NS_ENSURE_ARG(col);

    ErrorResult rv;
    GetCellValue(aRow, *col, aResult, rv);
    return rv.StealNSResult();
}

void
nsXULTreeBuilder::GetCellText(int32_t aRow, nsTreeColumn& aColumn,
                              nsAString& aText, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return;
    }

    // Find the <cell> that corresponds to the column we want.
    nsIContent* cell = GetTemplateActionCellFor(aRow, aColumn);
    if (cell) {
        nsAutoString raw;
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::label, raw);

        SubstituteText(mRows[aRow]->mMatch->mResult, raw, aText);
    } else {
        aText.Truncate();
    }
}

NS_IMETHODIMP
nsXULTreeBuilder::GetCellText(int32_t aRow, nsITreeColumn* aCol, nsAString& aResult)
{
    RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
    NS_ENSURE_ARG(col);

    ErrorResult rv;
    GetCellText(aRow, *col, aResult, rv);
    return rv.StealNSResult();
}

void
nsXULTreeBuilder::SetTree(TreeBoxObject* aTree, ErrorResult& aError)
{
    aError = SetTree(aTree);
}

NS_IMETHODIMP
nsXULTreeBuilder::SetTree(nsITreeBoxObject* aTree)
{
    mBoxObject = aTree;

    // If this is teardown time, then we're done.
    if (!mBoxObject) {
        Uninit(false);
        return NS_OK;
    }
    NS_ENSURE_TRUE(mRoot, NS_ERROR_NOT_INITIALIZED);

    // Only use the XUL store if the root's principal is trusted.
    if (mRoot->NodePrincipal()->GetIsSystemPrincipal()) {
        mLocalStore = do_GetService("@mozilla.org/xul/xulstore;1");
        if (NS_WARN_IF(!mLocalStore)) {
            return NS_ERROR_NOT_INITIALIZED;
        }
    }

    Rebuild();

    EnsureSortVariables();
    if (mSortVariable)
        SortSubtree(mRows.GetRoot());

    return NS_OK;
}

void
nsXULTreeBuilder::ToggleOpenState(int32_t aRow, ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return;
    }

    nsIXULTemplateResult* result = mRows[aRow]->mMatch->mResult;
    if (!result) {
        aError.Throw(NS_ERROR_FAILURE);
        return;
    }

    if (mFlags & eDontRecurse) {
        return;
    }

    if (result && result != mRootResult) {
        // don't open containers if child processing isn't allowed
        bool mayProcessChildren;
        aError = result->GetMayProcessChildren(&mayProcessChildren);
        if (aError.Failed() || !mayProcessChildren) {
            return;
        }
    }

    uint32_t count = mObservers.Length();
    for (uint32_t i = 0; i < count; ++i) {
        nsCOMPtr<nsIXULTreeBuilderObserver> observer = mObservers.SafeElementAt(i);
        if (observer)
            observer->OnToggleOpenState(aRow);
    }

    if (mLocalStore && mRoot) {
        bool isOpen;
        IsContainerOpen(aRow, &isOpen);

        nsIDocument* doc = mRoot->GetComposedDoc();
        if (!doc) {
            aError.Throw(NS_ERROR_FAILURE);
            return;
        }

        nsIURI* docURI = doc->GetDocumentURI();
        nsTreeRows::Row& row = *(mRows[aRow]);
        nsAutoString nodeid;
        aError = row.mMatch->mResult->GetId(nodeid);
        if (aError.Failed()) {
            return;
        }

        nsAutoCString utf8uri;
        aError = docURI->GetSpec(utf8uri);
        if (NS_WARN_IF(aError.Failed())) {
            return;
        }
        NS_ConvertUTF8toUTF16 uri(utf8uri);

        if (isOpen) {
            mLocalStore->RemoveValue(uri, nodeid, NS_LITERAL_STRING("open"));
            CloseContainer(aRow);
        } else {
            mLocalStore->SetValue(uri, nodeid, NS_LITERAL_STRING("open"),
                NS_LITERAL_STRING("true"));

            OpenContainer(aRow, result);
        }
    }
}

NS_IMETHODIMP
nsXULTreeBuilder::ToggleOpenState(int32_t aIndex)
{
    ErrorResult rv;
    ToggleOpenState(aIndex, rv);
    return rv.StealNSResult();
}

void
nsXULTreeBuilder::CycleHeader(nsTreeColumn& aColumn, ErrorResult& aError)
{
    nsCOMPtr<nsIDOMElement> element;
    aColumn.GetElement(getter_AddRefs(element));

    nsAutoString id;
    aColumn.GetId(id);

    uint32_t count = mObservers.Length();
    for (uint32_t i = 0; i < count; ++i) {
        nsCOMPtr<nsIXULTreeBuilderObserver> observer = mObservers.SafeElementAt(i);
        if (observer)
            observer->OnCycleHeader(id.get(), element);
    }

    aError = Sort(element);
}

NS_IMETHODIMP
nsXULTreeBuilder::CycleHeader(nsITreeColumn* aCol)
{
    RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
    NS_ENSURE_ARG(col);

    ErrorResult rv;
    CycleHeader(*col, rv);
    return rv.StealNSResult();
}

NS_IMETHODIMP
nsXULTreeBuilder::SelectionChanged()
{
    uint32_t count = mObservers.Length();
    for (uint32_t i = 0; i < count; ++i) {
        nsCOMPtr<nsIXULTreeBuilderObserver> observer = mObservers.SafeElementAt(i);
        if (observer)
            observer->OnSelectionChanged();
    }

    return NS_OK;
}

void
nsXULTreeBuilder::CycleCell(int32_t aRow, nsTreeColumn& aColumn)
{
    nsAutoString id;
    aColumn.GetId(id);

    uint32_t count = mObservers.Length();
    for (uint32_t i = 0; i < count; ++i) {
        nsCOMPtr<nsIXULTreeBuilderObserver> observer = mObservers.SafeElementAt(i);
        if (observer)
            observer->OnCycleCell(aRow, id.get());
    }
}

NS_IMETHODIMP
nsXULTreeBuilder::CycleCell(int32_t aRow, nsITreeColumn* aCol)
{
    RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
    NS_ENSURE_ARG(col);

    CycleCell(aRow, *col);
    return NS_OK;
}

bool
nsXULTreeBuilder::IsEditable(int32_t aRow, nsTreeColumn& aColumn,
                             ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return false;
    }

    // Find the <cell> that corresponds to the column we want.
    nsIContent* cell = GetTemplateActionCellFor(aRow, aColumn);
    if (!cell) {
        return true;
    }

    nsAutoString raw;
    cell->GetAttr(kNameSpaceID_None, nsGkAtoms::editable, raw);

    nsAutoString editable;
    SubstituteText(mRows[aRow]->mMatch->mResult, raw, editable);

    return !editable.EqualsLiteral("false");
}

NS_IMETHODIMP
nsXULTreeBuilder::IsEditable(int32_t aRow, nsITreeColumn* aCol, bool* _retval)
{
    RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
    NS_ENSURE_ARG(col);

    ErrorResult rv;
    *_retval = IsEditable(aRow, *col, rv);
    return rv.StealNSResult();
}

bool
nsXULTreeBuilder::IsSelectable(int32_t aRow, nsTreeColumn& aColumn,
                               ErrorResult& aError)
{
    if (!IsValidRowIndex(aRow)) {
        aError.Throw(NS_ERROR_INVALID_ARG);
        return false;
    }

    // Find the <cell> that corresponds to the column we want.
    nsIContent* cell = GetTemplateActionCellFor(aRow, aColumn);
    if (!cell) {
        return true;
    }

    nsAutoString raw;
    cell->GetAttr(kNameSpaceID_None, nsGkAtoms::selectable, raw);

    nsAutoString selectable;
    SubstituteText(mRows[aRow]->mMatch->mResult, raw, selectable);

    return !selectable.EqualsLiteral("false");
}

NS_IMETHODIMP
nsXULTreeBuilder::IsSelectable(int32_t aRow, nsITreeColumn* aCol, bool* _retval)
{
    RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
    NS_ENSURE_ARG(col);

    ErrorResult rv;
    *_retval = IsSelectable(aRow, *col, rv);
    return rv.StealNSResult();
}

NS_IMETHODIMP
nsXULTreeBuilder::SetCellValue(int32_t aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
    NS_ENSURE_ARG_POINTER(aCol);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeBuilder::SetCellText(int32_t aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
    NS_ENSURE_ARG_POINTER(aCol);
    return NS_OK;
}

void
nsXULTreeBuilder::PerformAction(const nsAString& aAction)
{
    uint32_t count = mObservers.Length();
    for (uint32_t i = 0; i < count; ++i) {
        nsCOMPtr<nsIXULTreeBuilderObserver> observer = mObservers.SafeElementAt(i);
        if (observer) {
            observer->OnPerformAction(PromiseFlatString(aAction).get());
        }
    }
}

NS_IMETHODIMP
nsXULTreeBuilder::PerformAction(const char16_t* aAction)
{
    PerformAction(aAction ? nsDependentString(aAction) : EmptyString());

    return NS_OK;
}

void
nsXULTreeBuilder::PerformActionOnRow(const nsAString& aAction, int32_t aRow)
{
    uint32_t count = mObservers.Length();
    for (uint32_t i = 0; i < count; ++i) {
        nsCOMPtr<nsIXULTreeBuilderObserver> observer = mObservers.SafeElementAt(i);
        if (observer) {
            observer->OnPerformActionOnRow(PromiseFlatString(aAction).get(), aRow);
        }
    }
}

NS_IMETHODIMP
nsXULTreeBuilder::PerformActionOnRow(const char16_t* aAction, int32_t aRow)
{
    PerformActionOnRow(aAction ? nsDependentString(aAction) : EmptyString(), aRow);

    return NS_OK;
}

void
nsXULTreeBuilder::PerformActionOnCell(const nsAString& aAction, int32_t aRow,
                                      nsTreeColumn& aColumn)
{
    nsAutoString id;
    aColumn.GetId(id);

    uint32_t count = mObservers.Length();
    for (uint32_t i = 0; i < count; ++i) {
        nsCOMPtr<nsIXULTreeBuilderObserver> observer = mObservers.SafeElementAt(i);
        if (observer) {
            observer->OnPerformActionOnCell(PromiseFlatString(aAction).get(), aRow, id.get());
        }
    }
}

NS_IMETHODIMP
nsXULTreeBuilder::PerformActionOnCell(const char16_t* aAction, int32_t aRow, nsITreeColumn* aCol)
{
    RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
    NS_ENSURE_ARG(col);

    PerformActionOnCell(aAction ? nsDependentString(aAction) : EmptyString(), aRow, *col);

    return NS_OK;
}


void
nsXULTreeBuilder::NodeWillBeDestroyed(const nsINode* aNode)
{
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
    mObservers.Clear();

    nsXULTemplateBuilder::NodeWillBeDestroyed(aNode);
}

bool
nsXULTreeBuilder::HasGeneratedContent(nsIRDFResource* aResource,
                                      const nsAString& aTag,
                                      ErrorResult& aError)
{
    if (!aResource) {
        aError.Throw(NS_ERROR_INVALID_POINTER);
        return false;
    }

    if (!mRootResult) {
        return false;
    }

    nsCOMPtr<nsIRDFResource> rootresource;
    aError = mRootResult->GetResource(getter_AddRefs(rootresource));
    if (aError.Failed()) {
        return false;
    }

    return aResource == rootresource ||
           mRows.FindByResource(aResource) != mRows.Last();
}

bool
nsXULTreeBuilder::GetInsertionLocations(nsIXULTemplateResult* aResult,
                                        nsCOMArray<nsIContent>** aLocations)
{
    *aLocations = nullptr;

    // Get the reference point and check if it is an open container. Rows
    // should not be generated otherwise.

    nsAutoString ref;
    nsresult rv = aResult->GetBindingFor(mRefVariable, ref);
    if (NS_FAILED(rv) || ref.IsEmpty())
        return false;

    nsCOMPtr<nsIRDFResource> container;
    rv = gRDFService->GetUnicodeResource(ref, getter_AddRefs(container));
    if (NS_FAILED(rv))
        return false;

    // Can always insert into the root resource
    if (container == mRows.GetRootResource())
        return true;

    nsTreeRows::iterator iter = mRows.FindByResource(container);
    if (iter == mRows.Last())
        return false;

    return (iter->mContainerState == nsTreeRows::eContainerState_Open);
}

struct ResultComparator
{
    nsXULTreeBuilder* const mTreebuilder;
    nsIXULTemplateResult* const mResult;
    ResultComparator(nsXULTreeBuilder* aTreebuilder, nsIXULTemplateResult* aResult)
      : mTreebuilder(aTreebuilder), mResult(aResult) {}
    int operator()(const nsTreeRows::Row& aSubtree) const {
        return mTreebuilder->CompareResults(mResult, aSubtree.mMatch->mResult);
    }
};

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
        int32_t row = iter.GetRowIndex();

        // If the row contains children, remove the matches from the
        // children so that they can be regenerated again if the element
        // gets added back.
        int32_t delta = mRows.GetSubtreeSizeFor(iter);
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
        int32_t row = -1;
        nsTreeRows::Subtree* parent = nullptr;
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
            size_t index = parent->Count();

            if (mSortVariable) {
                // Figure out where to put the new element through
                // binary search.
                mozilla::BinarySearchIf(*parent, 0, parent->Count(),
                                        ResultComparator(this, result), &index);
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

            if (IsContainerOpen(result)) {
                OpenContainer(iter.GetRowIndex(), result);
            }
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

        int32_t row = iter.GetRowIndex();
        if (row >= 0)
            mBoxObject->InvalidateRow(row);

        MOZ_LOG(gXULTemplateLog, LogLevel::Debug,
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
                    mSortVariable = NS_Atomize(sort);

                    static nsIContent::AttrValuesArray strings[] =
                      {&nsGkAtoms::ascending, &nsGkAtoms::descending, nullptr};
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

    nsCOMPtr<nsIDocument> doc = mRoot->GetComposedDoc();

    // Bail out early if we are being torn down.
    if (!doc)
        return NS_OK;

    if (! mQueryProcessor)
        return NS_OK;

    if (mBoxObject) {
        mBoxObject->BeginUpdateBatch();
    }

    if (mQueriesCompiled) {
        Uninit(false);
    }
    else if (mBoxObject) {
        int32_t count = mRows.Count();
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
nsXULTreeBuilder::GetTemplateActionRowFor(int32_t aRow, nsIContent** aResult)
{
    // Get the template in the DOM from which we're supposed to
    // generate text
    nsTreeRows::Row& row = *(mRows[aRow]);

    // The match stores the indices of the rule and query to use. Use these
    // to look up the right nsTemplateRule and use that rule's action to get
    // the treerow in the template.
    int16_t ruleindex = row.mMatch->RuleIndex();
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

    *aResult = nullptr;
    return NS_OK;
}

nsIContent*
nsXULTreeBuilder::GetTemplateActionCellFor(int32_t aRow, nsTreeColumn& aCol)
{
    nsCOMPtr<nsIContent> row;
    GetTemplateActionRowFor(aRow, getter_AddRefs(row));
    if (!row) {
        return nullptr;
    }

    nsCOMPtr<nsIAtom> colAtom(aCol.GetAtom());
    int32_t colIndex(aCol.GetIndex());

    nsIContent* result = nullptr;
    uint32_t j = 0;
    for (nsIContent* child = row->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
        if (child->NodeInfo()->Equals(nsGkAtoms::treecell, kNameSpaceID_XUL)) {
            if (colAtom &&
                child->AttrValueIs(kNameSpaceID_None, nsGkAtoms::ref, colAtom,
                                   eCaseMatters)) {
                return child;
            }

            if (j == (uint32_t)colIndex) {
                result = child;
            }
            ++j;
        }
    }
    return result;
}

nsresult
nsXULTreeBuilder::GetResourceFor(int32_t aRow, nsIRDFResource** aResource)
{
    nsTreeRows::Row& row = *(mRows[aRow]);
    return GetResultResource(row.mMatch->mResult, aResource);
}

nsresult
nsXULTreeBuilder::OpenContainer(int32_t aIndex, nsIXULTemplateResult* aResult)
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

    int32_t count;
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
                                int32_t aIndex,
                                nsIXULTemplateResult *aResult,
                                int32_t* aDelta)
{
    AutoTArray<int32_t, 8> open;
    int32_t count = 0;

    int32_t rulecount = mQuerySets.Length();

    for (int32_t r = 0; r < rulecount; r++) {
        nsTemplateQuerySet* queryset = mQuerySets[r];
        OpenSubtreeForQuerySet(aSubtree, aIndex, aResult, queryset, &count, open);
    }

    // Now recursively deal with any open sub-containers that just got
    // inserted. We need to do this back-to-front to avoid skewing offsets.
    for (int32_t i = open.Length() - 1; i >= 0; --i) {
        int32_t index = open[i];

        nsTreeRows::Subtree* child =
            mRows.EnsureSubtreeFor(aSubtree, index);

        nsIXULTemplateResult* result = (*aSubtree)[index].mMatch->mResult;

        int32_t delta;
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
                                         int32_t aIndex,
                                         nsIXULTemplateResult* aResult,
                                         nsTemplateQuerySet* aQuerySet,
                                         int32_t* aDelta,
                                         nsTArray<int32_t>& open)
{
    int32_t count = *aDelta;

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

        nsTemplateMatch* prevmatch = nullptr;
        nsTemplateMatch* existingmatch = nullptr;
        if (mMatchMap.Get(resultid, &existingmatch)){
            // check if there is an existing match that matched a rule
            while (existingmatch) {
                if (existingmatch->IsActive())
                    generateContent = false;
                prevmatch = existingmatch;
                existingmatch = existingmatch->mNext;
            }
        }

        nsTemplateMatch *newmatch =
            nsTemplateMatch::Create(aQuerySet->Priority(), nextresult, nullptr);
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
                        nsTemplateMatch::Destroy(newmatch, false);
                        return rv;
                    }

                    if (resultid == parentid) {
                        cyclic = true;
                        break;
                    }
                }
            }

            if (cyclic) {
                NS_WARNING("tree cannot handle cyclic graphs");
                nsTemplateMatch::Destroy(newmatch, false);
                continue;
            }

            int16_t ruleindex;
            nsTemplateRule* matchedrule = nullptr;
            rv = DetermineMatchedRule(nullptr, nextresult, aQuerySet,
                                      &matchedrule, &ruleindex);
            if (NS_FAILED(rv)) {
                nsTemplateMatch::Destroy(newmatch, false);
                return rv;
            }

            if (matchedrule) {
                rv = newmatch->RuleMatched(aQuerySet, matchedrule, ruleindex,
                                           nextresult);
                if (NS_FAILED(rv)) {
                    nsTemplateMatch::Destroy(newmatch, false);
                    return rv;
                }

                // Remember that this match applied to this row
                mRows.InsertRowAt(newmatch, aSubtree, count);

                // If this is open, then remember it so we can recursively add
                // *its* rows to the tree.
                if (IsContainerOpen(nextresult)) {
                    if (open.AppendElement(count) == nullptr)
                        return NS_ERROR_OUT_OF_MEMORY;
                }

                ++count;
            }

            if (mFlags & eLoggingEnabled)
                OutputMatchToLog(resultid, newmatch, true);

        }

        if (prevmatch) {
            prevmatch->mNext = newmatch;
        }
        else {
            mMatchMap.Put(resultid, newmatch);
        }
    }

    *aDelta = count;
    return rv;
}

nsresult
nsXULTreeBuilder::CloseContainer(int32_t aIndex)
{
    NS_ASSERTION(aIndex >= 0 && aIndex < mRows.Count(), "bad row");
    if (aIndex < 0 || aIndex >= mRows.Count())
        return NS_ERROR_INVALID_ARG;

    nsTreeRows::iterator iter = mRows[aIndex];

    if (iter->mSubtree)
        RemoveMatchesFor(*iter->mSubtree);


    int32_t count = mRows.GetSubtreeSizeFor(iter);
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
    for (int32_t i = subtree.Count() - 1; i >= 0; --i) {
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
                nsTemplateMatch::Destroy(existingmatch, true);
                existingmatch = nextmatch;
            }

            mMatchMap.Remove(id);
        }

        if ((row.mContainerState == nsTreeRows::eContainerState_Open) && row.mSubtree)
            RemoveMatchesFor(*(row.mSubtree));
    }

    return NS_OK;
}


bool
nsXULTreeBuilder::IsContainerOpen(nsIXULTemplateResult *aResult)
{
  // items are never open if recursion is disabled
  if ((mFlags & eDontRecurse) && aResult != mRootResult) {
    return false;
  }

  if (!mLocalStore) {
    return false;
  }

  nsIDocument* doc = mRoot->GetComposedDoc();
  if (!doc) {
    return false;
  }

  nsIURI* docURI = doc->GetDocumentURI();

  nsAutoString nodeid;
  nsresult rv = aResult->GetId(nodeid);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsAutoCString utf8uri;
  rv = docURI->GetSpec(utf8uri);
  if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
  }
  NS_ConvertUTF8toUTF16 uri(utf8uri);

  nsAutoString val;
  mLocalStore->GetValue(uri, nodeid, NS_LITERAL_STRING("open"), val);
  return val.EqualsLiteral("true");
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

int32_t
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
            return 0;

        nsCOMPtr<nsIRDFResource> container = do_QueryInterface(ref);
        if (container) {
            bool isSequence = false;
            gRDFContainerUtils->IsSeq(mDB, container, &isSequence);
            if (isSequence) {
                // Determine the indices of the left and right elements
                // in the container.
                int32_t lindex = 0, rindex = 0;

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

    int32_t sortorder;
    if (!mQueryProcessor)
        return 0;

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

    for (int32_t i = aSubtree->Count() - 1; i >= 0; --i) {
        nsTreeRows::Subtree* child = (*aSubtree)[i].mSubtree;
        if (child)
            SortSubtree(child);
    }

    return NS_OK;
}

bool
nsXULTreeBuilder::CanDrop(int32_t aRow, int32_t aOrientation,
                          DataTransfer* aDataTransfer, ErrorResult& aError)
{
    uint32_t count = mObservers.Length();
    for (uint32_t i = 0; i < count; ++i) {
        nsCOMPtr<nsIXULTreeBuilderObserver> observer = mObservers.SafeElementAt(i);
        if (observer) {
            bool canDrop = false;
            observer->CanDrop(aRow, aOrientation, aDataTransfer, &canDrop);
            if (canDrop) {
                return true;
            }
        }
    }

    return false;
}

NS_IMETHODIMP
nsXULTreeBuilder::CanDrop(int32_t index, int32_t orientation,
                          nsIDOMDataTransfer* dataTransfer, bool *_retval)
{
    ErrorResult rv;
    *_retval = CanDrop(index, orientation, DataTransfer::Cast(dataTransfer),
                       rv);
    return rv.StealNSResult();
}

void
nsXULTreeBuilder::Drop(int32_t aRow, int32_t aOrientation,
                       DataTransfer* aDataTransfer, ErrorResult& aError)
{
    uint32_t count = mObservers.Length();
    for (uint32_t i = 0; i < count; ++i) {
        nsCOMPtr<nsIXULTreeBuilderObserver> observer = mObservers.SafeElementAt(i);
        if (observer) {
            bool canDrop = false;
            observer->CanDrop(aRow, aOrientation, aDataTransfer, &canDrop);
            if (canDrop)
                observer->OnDrop(aRow, aOrientation, aDataTransfer);
        }
    }
}

NS_IMETHODIMP
nsXULTreeBuilder::Drop(int32_t row, int32_t orient, nsIDOMDataTransfer* dataTransfer)
{
    ErrorResult rv;
    Drop(row, orient, DataTransfer::Cast(dataTransfer), rv);
    return rv.StealNSResult();
}

NS_IMETHODIMP
nsXULTreeBuilder::IsSorted(bool *_retval)
{
  *_retval = (mSortVariable != nullptr);
  return NS_OK;
}

bool
nsXULTreeBuilder::IsValidRowIndex(int32_t aRowIndex)
{
    return aRowIndex >= 0 && aRowIndex < int32_t(mRows.Count());
}
