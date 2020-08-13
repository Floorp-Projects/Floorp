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

using namespace mozilla;
using mozilla::dom::BrowsingContext;
using mozilla::dom::Document;
using mozilla::dom::Element;
using mozilla::dom::Selection;

//---------------------------------------------------
//-- nsPrintObject Class Impl
//---------------------------------------------------
nsPrintObject::nsPrintObject()
    : mContent(nullptr),
      mFrameType(eFrame),
      mParent(nullptr),
      mHasBeenPrinted(false),
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

  // If we are cloning from a document in a different BrowsingContext, we need
  // to make sure to copy over our opener policy information from that
  // BrowsingContext.
  BrowsingContext* targetBC = mDocShell->GetBrowsingContext();
  BrowsingContext* sourceBC = aDoc->GetBrowsingContext();
  NS_ENSURE_STATE(sourceBC);
  if (targetBC != sourceBC) {
    MOZ_ASSERT(targetBC->IsTopContent());
    // In the case where the source is an iframe, this information needs to be
    // copied from the toplevel source BrowsingContext, as we may be making a
    // static clone of a single subframe.
    MOZ_ALWAYS_SUCCEEDS(
        targetBC->SetOpenerPolicy(sourceBC->Top()->GetOpenerPolicy()));
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

  // "frame" elements not in a frameset context should be treated
  // as iframes
  if (mContent->IsHTMLElement(nsGkAtoms::frame) &&
      mParent->mFrameType == eFrameSet) {
    mFrameType = eFrame;
  } else {
    // Assume something iframe-like, i.e. iframe, object, or embed
    mFrameType = eIFrame;
  }

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

void nsPrintObject::EnablePrinting(bool aEnable) {
  mPrintingIsEnabled = aEnable;

  for (const UniquePtr<nsPrintObject>& kid : mKids) {
    kid->EnablePrinting(aEnable);
  }
}

bool nsPrintObject::HasSelection() const {
  return mDocument && mDocument->GetProperty(nsGkAtoms::printselectionranges);
}

void nsPrintObject::EnablePrintingSelectionOnly() {
  mPrintingIsEnabled = HasSelection();

  for (const UniquePtr<nsPrintObject>& kid : mKids) {
    kid->EnablePrintingSelectionOnly();
  }
}
