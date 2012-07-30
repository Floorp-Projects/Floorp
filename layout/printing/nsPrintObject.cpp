/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintObject.h"
#include "nsIContentViewer.h"
#include "nsIDOMDocument.h"
#include "nsContentUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsPIDOMWindow.h"
#include "nsGkAtoms.h"
#include "nsComponentManagerUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsIBaseWindow.h"
                                                   
//---------------------------------------------------
//-- nsPrintObject Class Impl
//---------------------------------------------------
nsPrintObject::nsPrintObject() :
  mContent(nullptr), mFrameType(eFrame), mParent(nullptr),
  mHasBeenPrinted(false), mDontPrint(true), mPrintAsIs(false),
  mSharedPresShell(false), mInvisible(false), mDidCreateDocShell(false),
  mShrinkRatio(1.0), mZoomRatio(1.0)
{
  MOZ_COUNT_CTOR(nsPrintObject);
}


nsPrintObject::~nsPrintObject()
{
  MOZ_COUNT_DTOR(nsPrintObject);
  for (PRUint32 i=0;i<mKids.Length();i++) {
    nsPrintObject* po = mKids[i];
    delete po;
  }

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
    nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(aDocShell);
    PRInt32 itemType = 0;
    item->GetItemType(&itemType);
    // Create a container docshell for printing.
    mDocShell = do_CreateInstance("@mozilla.org/docshell;1");
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_OUT_OF_MEMORY);
    mDidCreateDocShell = true;
    nsCOMPtr<nsIDocShellTreeItem> newItem = do_QueryInterface(mDocShell);
    newItem->SetItemType(itemType);
    newItem->SetTreeOwner(mTreeOwner);
  }
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDocument> dummy = do_GetInterface(mDocShell);
  nsCOMPtr<nsIContentViewer> viewer;
  mDocShell->GetContentViewer(getter_AddRefs(viewer));
  NS_ENSURE_STATE(viewer);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  NS_ENSURE_STATE(doc);

  if (mParent) {
    nsCOMPtr<nsPIDOMWindow> window = doc->GetWindow();
    if (window) {
      mContent = do_QueryInterface(window->GetFrameElementInternal());
    }
    mDocument = doc;
    return NS_OK;
  }

  mDocument = doc->CreateStaticClone(mDocShell);
  nsCOMPtr<nsIDOMDocument> clonedDOMDoc = do_QueryInterface(mDocument);
  NS_ENSURE_STATE(clonedDOMDoc);

  viewer->SetDOMDocument(clonedDOMDoc);
  return NS_OK;
}

//------------------------------------------------------------------
// Resets PO by destroying the presentation
void 
nsPrintObject::DestroyPresentation()
{
  mPresContext = nullptr;
  if (mPresShell) {
    mPresShell->EndObservingDocument();
    nsAutoScriptBlocker scriptBlocker;
    mPresShell->Destroy();
  }
  mPresShell   = nullptr;
  mViewManager = nullptr;
}

