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
nsPrintObject::nsPrintObject(nsIDocShell& aDocShell, Document& aDoc,
                             nsPrintObject* aParent)
    : mDocShell(&aDocShell), mDocument(&aDoc), mParent(aParent) {
  MOZ_COUNT_CTOR(nsPrintObject);
  MOZ_ASSERT(aDoc.IsStaticDocument());

  if (!aParent) {
    // We are a root nsPrintObject.
    // Ensure the document has no presentation.
    DestroyPresentation();
  } else {
    // We are a nested nsPrintObject.
    nsCOMPtr<nsPIDOMWindowOuter> window = aDoc.GetWindow();
    mContent = window->GetFrameElementInternal();
  }
}

nsPrintObject::~nsPrintObject() {
  MOZ_COUNT_DTOR(nsPrintObject);

  DestroyPresentation();
  mDocShell = nullptr;
  mTreeOwner = nullptr;  // mTreeOwner must be released after mDocShell;
}

//------------------------------------------------------------------
// Resets PO by destroying the presentation
void nsPrintObject::DestroyPresentation() {
  if (mDocument) {
    if (RefPtr<PresShell> ps = mDocument->GetPresShell()) {
      MOZ_DIAGNOSTIC_ASSERT(!mPresShell || ps == mPresShell);
      mPresShell = nullptr;
      nsAutoScriptBlocker scriptBlocker;
      ps->EndObservingDocument();
      ps->Destroy();
    }
  }
  mPresShell = nullptr;
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
