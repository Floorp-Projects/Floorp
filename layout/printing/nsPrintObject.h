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
#include "nsIPresShell.h"
#include "nsViewManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"

class nsIContent;
class nsIDocument;
class nsPresContext;

// nsPrintObject Document Type
enum PrintObjectType  {eDoc = 0, eFrame = 1, eIFrame = 2, eFrameSet = 3};

//---------------------------------------------------
//-- nsPrintObject Class
//---------------------------------------------------
class nsPrintObject
{

public:
  nsPrintObject();
  ~nsPrintObject(); // non-virtual

  // Methods
  nsresult Init(nsIDocShell* aDocShell, nsIDocument* aDoc, bool aPrintPreview);

  bool IsPrintable()  { return !mDontPrint; }
  void   DestroyPresentation();

  // Data Members
  nsCOMPtr<nsIDocShell>    mDocShell;
  nsCOMPtr<nsIDocShellTreeOwner> mTreeOwner;
  nsCOMPtr<nsIDocument>    mDocument;

  RefPtr<nsPresContext>  mPresContext;
  nsCOMPtr<nsIPresShell>   mPresShell;
  RefPtr<nsViewManager> mViewManager;

  nsCOMPtr<nsIContent>     mContent;
  PrintObjectType  mFrameType;

  nsTArray<mozilla::UniquePtr<nsPrintObject>> mKids;
  nsPrintObject*   mParent; // This is a non-owning pointer.
  bool             mHasBeenPrinted;
  bool             mDontPrint;
  bool             mPrintAsIs;
  bool             mInvisible;        // Indicates PO is set to not visible by CSS
  bool             mPrintPreview;
  bool             mDidCreateDocShell;
  float            mShrinkRatio;
  float            mZoomRatio;

private:
  nsPrintObject& operator=(const nsPrintObject& aOther) = delete;
};



#endif /* nsPrintObject_h___ */

