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
      mPrintPreview(false),
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
nsresult nsPrintObject::Init(nsIDocShell* aDocShell, Document* aDoc,
                             bool aPrintPreview) {
  NS_ENSURE_STATE(aDoc);

  mPrintPreview = aPrintPreview;

  if (mPrintPreview || mParent) {
    mDocShell = aDocShell;
  } else {
    mTreeOwner = do_GetInterface(aDocShell);

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
    mDocShell->SetTreeOwner(mTreeOwner);
  }
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  // Keep the document related to this docshell alive
  nsCOMPtr<Document> dummy = do_GetInterface(mDocShell);
  mozilla::Unused << dummy;

  nsCOMPtr<nsIContentViewer> viewer;
  mDocShell->GetContentViewer(getter_AddRefs(viewer));
  NS_ENSURE_STATE(viewer);

  if (mParent) {
    nsCOMPtr<nsPIDOMWindowOuter> window = aDoc->GetWindow();
    if (window) {
      mContent = window->GetFrameElementInternal();
    }
    mDocument = aDoc;
    return NS_OK;
  }

  mDocument = aDoc->CreateStaticClone(mDocShell);
  NS_ENSURE_STATE(mDocument);

  viewer->SetDocument(mDocument);
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
