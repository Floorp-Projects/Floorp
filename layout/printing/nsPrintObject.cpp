/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPrintObject.h"
#include "nsIContentViewer.h"
#include "nsIDOMDocument.h"

//---------------------------------------------------
//-- nsPrintObject Class Impl
//---------------------------------------------------
nsPrintObject::nsPrintObject() :
  mContent(nsnull), mFrameType(eFrame), mParent(nsnull),
  mHasBeenPrinted(PR_FALSE), mDontPrint(PR_TRUE), mPrintAsIs(PR_FALSE),
  mSharedPresShell(PR_FALSE), mInvisible(PR_FALSE),
  mShrinkRatio(1.0), mZoomRatio(1.0)
{
}


nsPrintObject::~nsPrintObject()
{
  for (PRInt32 i=0;i<mKids.Count();i++) {
    nsPrintObject* po = (nsPrintObject*)mKids[i];
    delete po;
  }

  DestroyPresentation();
}

//------------------------------------------------------------------
// Resets PO by destroying the presentation
nsresult 
nsPrintObject::Init(nsIDocShell* aDocShell)
{
  mDocShell = aDocShell;
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
  
  nsresult rv;
  nsCOMPtr<nsIContentViewer> viewer;
  rv = mDocShell->GetContentViewer(getter_AddRefs(viewer));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMDocument> doc;
  viewer->GetDOMDocument(getter_AddRefs(doc));
  NS_ENSURE_SUCCESS(rv, rv);
  
  mDocument = do_QueryInterface(doc);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  return NS_OK;
}

//------------------------------------------------------------------
// Resets PO by destroying the presentation
void 
nsPrintObject::DestroyPresentation()
{
  mWindow      = nsnull;
  mPresContext = nsnull;
  if (mPresShell) {
    mPresShell->EndObservingDocument();
    mPresShell->Destroy();
  }
  mPresShell   = nsnull;
  mViewManager = nsnull;
}

