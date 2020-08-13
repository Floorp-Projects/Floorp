/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPrintObject_h___
#define nsPrintObject_h___

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"

// Interfaces
#include "nsCOMPtr.h"
#include "nsViewManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"

class nsIContent;
class nsPresContext;

namespace mozilla {
class PresShell;
}  // namespace mozilla

// nsPrintObject Document Type
enum PrintObjectType { eDoc = 0, eFrame = 1, eIFrame = 2, eFrameSet = 3 };

//---------------------------------------------------
//-- nsPrintObject Class
//---------------------------------------------------
class nsPrintObject {
 public:
  nsPrintObject();
  ~nsPrintObject();  // non-virtual

  nsresult InitAsRootObject(nsIDocShell* aDocShell,
                            mozilla::dom::Document* aDoc,
                            bool aForPrintPreview);
  nsresult InitAsNestedObject(nsIDocShell* aDocShell,
                              mozilla::dom::Document* aDoc,
                              nsPrintObject* aParent);

  void DestroyPresentation();

  /**
   * Recursively sets all the PO items to be printed
   * from the given item down into the tree
   */
  void EnablePrinting(bool aEnable);

  /**
   * Recursively sets all the PO items to be printed if they have a selection.
   */
  void EnablePrintingSelectionOnly();

  bool PrintingIsEnabled() const { return mPrintingIsEnabled; }

  bool HasSelection() const;

  // Data Members
  nsCOMPtr<nsIDocShell> mDocShell;
  nsCOMPtr<nsIDocShellTreeOwner> mTreeOwner;
  RefPtr<mozilla::dom::Document> mDocument;

  RefPtr<nsPresContext> mPresContext;
  RefPtr<mozilla::PresShell> mPresShell;
  RefPtr<nsViewManager> mViewManager;

  nsCOMPtr<nsIContent> mContent;
  PrintObjectType mFrameType;

  nsTArray<mozilla::UniquePtr<nsPrintObject>> mKids;
  nsPrintObject* mParent;  // This is a non-owning pointer.
  bool mHasBeenPrinted;
  bool mInvisible;  // Indicates PO is set to not visible by CSS
  bool mDidCreateDocShell;
  float mShrinkRatio;
  float mZoomRatio;

 private:
  nsPrintObject& operator=(const nsPrintObject& aOther) = delete;

  bool mPrintingIsEnabled = false;
};

#endif /* nsPrintObject_h___ */
