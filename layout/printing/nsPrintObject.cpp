/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintObject.h"
#include "nsIContentViewer.h"
#include "nsIDOMDocument.h"
#include "nsContentUtils.h" // for nsAutoScriptBlocker
#include "nsIInterfaceRequestorUtils.h"
#include "nsPIDOMWindow.h"
#include "nsGkAtoms.h"
#include "nsComponentManagerUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsIBaseWindow.h"
#include "nsIDocument.h"

//---------------------------------------------------
//-- nsPrintObject Class Impl
//---------------------------------------------------
nsPrintObject::nsPrintObject() :
  mContent(nullptr), mFrameType(eFrame), mParent(nullptr),
  mHasBeenPrinted(false), mDontPrint(true), mPrintAsIs(false),
  mInvisible(false), mDidCreateDocShell(false),
  mShrinkRatio(1.0), mZoomRatio(1.0)
{
  MOZ_COUNT_CTOR(nsPrintObject);
}


nsPrintObject::~nsPrintObject()
{
  MOZ_COUNT_DTOR(nsPrintObject);

  DestroyPresentation();
  if (mDidCreateDocShell && mDocShell) {
    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(mDocShell));
    if (baseWin) {
      baseWin->Destroy();
    }
  }
  mDocShell = nullptr;
  mTreeOwner = nullptr; // mTreeOwner must be released after mDocShell;
}

//------------------------------------------------------------------
nsresult
nsPrintObject::Init(nsIDocShell* aDocShell, nsIDOMDocument* aDoc,
                    bool aPrintPreview)
{
  mPrintPreview = aPrintPreview;

  if (mPrintPreview || mParent) {
    mDocShell = aDocShell;
  } else {
    mTreeOwner = do_GetInterface(aDocShell);
    // Create a container docshell for printing.
    mDocShell = do_CreateInstance("@mozilla.org/docshell;1");
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_OUT_OF_MEMORY);
    mDidCreateDocShell = true;
    mDocShell->SetItemType(aDocShell->ItemType());
    mDocShell->SetTreeOwner(mTreeOwner);
  }
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  // Keep the document related to this docshell alive
  nsCOMPtr<nsIDOMDocument> dummy = do_GetInterface(mDocShell);
  mozilla::Unused << dummy;

  nsCOMPtr<nsIContentViewer> viewer;
  mDocShell->GetContentViewer(getter_AddRefs(viewer));
  NS_ENSURE_STATE(viewer);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  NS_ENSURE_STATE(doc);

  if (mParent) {
    nsCOMPtr<nsPIDOMWindowOuter> window = doc->GetWindow();
    if (window) {
      mContent = window->GetFrameElementInternal();
    }
    mDocument = doc;
    return NS_OK;
  }

  mDocument = doc->CreateStaticClone(mDocShell);
  NS_ENSURE_STATE(mDocument);

  viewer->SetDocument(mDocument);
  return NS_OK;
}

//------------------------------------------------------------------
// Resets PO by destroying the presentation
void
nsPrintObject::DestroyPresentation()
{
  if (mPresShell) {
    mPresShell->EndObservingDocument();
    nsAutoScriptBlocker scriptBlocker;
    nsCOMPtr<nsIPresShell> shell = mPresShell;
    mPresShell = nullptr;
    shell->Destroy();
  }
  mPresContext = nullptr;
  mViewManager = nullptr;
}

