/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintObject.h"

#include "nsIContentViewer.h"
#include "nsContentUtils.h"  // for nsAutoScriptBlocker
#include "nsIInterfaceRequestorUtils.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsComponentManagerUtils.h"
#include "nsIBaseWindow.h"
#include "nsDocShell.h"

#include "mozilla/PresShell.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"

using mozilla::PresShell;
using mozilla::dom::BrowsingContext;
using mozilla::dom::Document;
using mozilla::dom::Element;

//---------------------------------------------------
//-- nsPrintObject Class Impl
//---------------------------------------------------
nsPrintObject::nsPrintObject()
    : mContent(nullptr),
      mFrameType(eFrame),
      mParent(nullptr),
      mHasBeenPrinted(false),
      mDontPrint(true),
      mPrintAsIs(false),
      mInvisible(false),
      mDidCreateDocShell(false),
      mShrinkRatio(1.0),
      mZoomRatio(1.0) {
  MOZ_COUNT_CTOR(nsPrintObject);
}

nsPrintObject::~nsPrintObject() {
  MOZ_COUNT_DTOR(nsPrintObject);

  DestroyPresentation();
  if (mDidCreateDocShell && mDocShell) {
    RefPtr<BrowsingContext> bc(mDocShell->GetBrowsingContext());
    nsDocShell::Cast(mDocShell)->Destroy();
    bc->Detach();
  }
  if (mDocument) {
    mDocument->RemoveProperty(nsGkAtoms::printselectionranges);
  }
  mDocShell = nullptr;
  mTreeOwner = nullptr;  // mTreeOwner must be released after mDocShell;
}

//------------------------------------------------------------------

/**
 * Retrieves the node in a static-clone document that corresponds to aOrigNOde,
 * which is a node in the original document from which aStaticClone was cloned.
 */
static nsINode* GetCorrespondingNodeInDocument(const nsINode* aOrigNode,
                                               Document& aStaticClone) {
  MOZ_ASSERT(aOrigNode);

  // Selections in anonymous subtrees aren't supported.
  if (aOrigNode->IsInAnonymousSubtree() || aOrigNode->IsInShadowTree()) {
    return nullptr;
  }

  nsTArray<int32_t> indexArray;
  const nsINode* child = aOrigNode;
  while (const nsINode* parent = child->GetParentNode()) {
    int32_t index = parent->ComputeIndexOf(child);
    MOZ_ASSERT(index >= 0);
    indexArray.AppendElement(index);
    child = parent;
  }
  MOZ_ASSERT(child->IsDocument());

  nsINode* correspondingNode = &aStaticClone;
  for (int32_t i : Reversed(indexArray)) {
    correspondingNode = correspondingNode->GetChildAt_Deprecated(i);
    NS_ENSURE_TRUE(correspondingNode, nullptr);
  }

  return correspondingNode;
}

/**
 * Caches the selection ranges from the source document onto the static clone in
 * case the "Print Selection Only" functionality is invoked.
 *
 * Note that we cannot use the selection obtained from
 * Document::GetOriginalDocument() since that selection may have mutated after
 * the print was invoked.
 *
 * Note also that because nsRange objects point into a specific document's
 * nodes, we cannot reuse an array of nsRange objects across multiple static
 * clone documents. For that reason we cache a new array of ranges on each
 * static clone that we create.
 */
static void CachePrintSelectionRanges(const Document& aSourceDoc,
                                      Document& aStaticClone) {
  MOZ_ASSERT(aStaticClone.IsStaticDocument());
  MOZ_ASSERT(!aStaticClone.GetProperty(nsGkAtoms::printselectionranges));

  const Selection* origSelection = nullptr;
  const nsTArray<RefPtr<nsRange>>* origRanges = nullptr;
  bool sourceDocIsStatic = aSourceDoc.IsStaticDocument();

  if (sourceDocIsStatic) {
    origRanges = static_cast<nsTArray<RefPtr<nsRange>>*>(
        aSourceDoc.GetProperty(nsGkAtoms::printselectionranges));
  } else if (PresShell* shell = aSourceDoc.GetPresShell()) {
    origSelection = shell->GetCurrentSelection(SelectionType::eNormal);
  }

  if (!origSelection && !origRanges) {
    return;
  }

  size_t rangeCount =
      sourceDocIsStatic ? origRanges->Length() : origSelection->RangeCount();
  auto* printRanges = new nsTArray<RefPtr<nsRange>>(rangeCount);

  for (size_t i = 0; i < rangeCount; ++i) {
    const nsRange* range = sourceDocIsStatic ? origRanges->ElementAt(i).get()
                                             : origSelection->GetRangeAt(i);
    nsINode* startContainer = range->GetStartContainer();
    nsINode* endContainer = range->GetEndContainer();

    if (!startContainer || !endContainer) {
      continue;
    }

    nsINode* startNode =
        GetCorrespondingNodeInDocument(startContainer, aStaticClone);
    nsINode* endNode =
        GetCorrespondingNodeInDocument(endContainer, aStaticClone);

    if (!startNode || !endNode) {
      continue;
    }

    RefPtr<nsRange> clonedRange =
        nsRange::Create(startNode, range->StartOffset(), endNode,
                        range->EndOffset(), IgnoreErrors());
    if (clonedRange && !clonedRange->Collapsed()) {
      printRanges->AppendElement(std::move(clonedRange));
    }
  }

  aStaticClone.SetProperty(nsGkAtoms::printselectionranges, printRanges,
                           nsINode::DeleteProperty<nsTArray<RefPtr<nsRange>>>);
}

nsresult nsPrintObject::InitAsRootObject(nsIDocShell* aDocShell, Document* aDoc,
                                         bool aForPrintPreview) {
  NS_ENSURE_STATE(aDocShell);
  NS_ENSURE_STATE(aDoc);

  if (aForPrintPreview) {
    nsCOMPtr<nsIContentViewer> viewer;
    aDocShell->GetContentViewer(getter_AddRefs(viewer));
    if (viewer && viewer->GetDocument() && viewer->GetDocument()->IsShowing()) {
      // We're about to discard this document, and it needs mIsShowing to be
      // false to avoid triggering the assertion in its dtor.
      viewer->GetDocument()->OnPageHide(false, nullptr);
    }
    mDocShell = aDocShell;
  } else {
    // When doing an actual print, we create a BrowsingContext/nsDocShell that
    // is detached from any browser window or tab.

    // Create a new BrowsingContext to create our DocShell in.
    RefPtr<BrowsingContext> bc = BrowsingContext::CreateIndependent(
        nsDocShell::Cast(aDocShell)->GetBrowsingContext()->GetType());

    // Create a container docshell for printing.
    mDocShell = nsDocShell::Create(bc);
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_OUT_OF_MEMORY);

    mDidCreateDocShell = true;
    MOZ_ASSERT(mDocShell->ItemType() == aDocShell->ItemType());

    mTreeOwner = do_GetInterface(aDocShell);
    mDocShell->SetTreeOwner(mTreeOwner);

    // Make sure nsDocShell::EnsureContentViewer() is called:
    mozilla::Unused << nsDocShell::Cast(mDocShell)->GetDocument();
  }

  mDocument = aDoc->CreateStaticClone(mDocShell);
  NS_ENSURE_STATE(mDocument);
  CachePrintSelectionRanges(*aDoc, *mDocument);

  nsCOMPtr<nsIContentViewer> viewer;
  mDocShell->GetContentViewer(getter_AddRefs(viewer));
  NS_ENSURE_STATE(viewer);
  viewer->SetDocument(mDocument);

  return NS_OK;
}

nsresult nsPrintObject::InitAsNestedObject(nsIDocShell* aDocShell,
                                           Document* aDoc,
                                           nsPrintObject* aParent) {
  NS_ENSURE_STATE(aDocShell);
  NS_ENSURE_STATE(aDoc);

  mParent = aParent;
  mDocShell = aDocShell;
  mDocument = aDoc;

  nsCOMPtr<nsPIDOMWindowOuter> window = aDoc->GetWindow();
  mContent = window->GetFrameElementInternal();

  return NS_OK;
}

//------------------------------------------------------------------
// Resets PO by destroying the presentation
void nsPrintObject::DestroyPresentation() {
  if (mPresShell) {
    mPresShell->EndObservingDocument();
    nsAutoScriptBlocker scriptBlocker;
    RefPtr<PresShell> presShell = mPresShell;
    mPresShell = nullptr;
    presShell->Destroy();
  }
  mPresContext = nullptr;
  mViewManager = nullptr;
}
