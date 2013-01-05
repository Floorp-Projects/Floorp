/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPrintObject_h___
#define nsPrintObject_h___

#include "mozilla/Attributes.h"

// Interfaces
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsStyleSet.h"
#include "nsIViewManager.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIDocShellTreeOwner.h"

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
  nsresult Init(nsIDocShell* aDocShell, nsIDOMDocument* aDoc,
                bool aPrintPreview);

  bool IsPrintable()  { return !mDontPrint; }
  void   DestroyPresentation();

  // Data Members
  nsCOMPtr<nsIDocShell>    mDocShell;
  nsCOMPtr<nsIDocShellTreeOwner> mTreeOwner;
  nsCOMPtr<nsIDocument>    mDocument;

  nsRefPtr<nsPresContext>  mPresContext;
  nsCOMPtr<nsIPresShell>   mPresShell;
  nsCOMPtr<nsIViewManager> mViewManager;

  nsCOMPtr<nsIContent>     mContent;
  PrintObjectType  mFrameType;
  
  nsTArray<nsPrintObject*> mKids;
  nsPrintObject*   mParent;
  bool             mHasBeenPrinted;
  bool             mDontPrint;
  bool             mPrintAsIs;
  bool             mSharedPresShell;
  bool             mInvisible;        // Indicates PO is set to not visible by CSS
  bool             mPrintPreview;
  bool             mDidCreateDocShell;
  float            mShrinkRatio;
  float            mZoomRatio;

private:
  nsPrintObject& operator=(const nsPrintObject& aOther) MOZ_DELETE;
};



#endif /* nsPrintObject_h___ */

