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
#include "nsIDocShellTreeItem.h"
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
    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(mDocShell));
    if (baseWin) {
      baseWin->Destroy();
    }
  }
  mDocShell = nullptr;
  mTreeOwner = nullptr;  // mTreeOwner must be released after mDocShell;
}

//------------------------------------------------------------------
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
    RefPtr<BrowsingContext> bc = BrowsingContext::Create(
        /* aParent */ nullptr,
        /* aOpener */ nullptr, EmptyString(),
        aDocShell->ItemType() == nsIDocShellTreeItem::typeContent
            ? BrowsingContext::Type::Content
            : BrowsingContext::Type::Chrome);

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
