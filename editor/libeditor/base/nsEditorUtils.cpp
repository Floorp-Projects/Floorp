/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "nsEditorUtils.h"
#include "nsIDOMDocument.h"


/******************************************************************************
 * nsAutoSelectionReset
 *****************************************************************************/

nsAutoSelectionReset::nsAutoSelectionReset(nsIDOMSelection *aSel, nsEditor *aEd) : 
mSel(nsnull)
,mEd(nsnull)
{ 
  if (!aSel || !aEd) return;    // not much we can do, bail.
  if (aEd->mSavedSel) return;   // we already have initted mSavedSel, so this must be nested call.
  mSel = do_QueryInterface(aSel);
  mEd = aEd;
  if (mSel)
  {
    mEd->mSavedSel = new nsSelectionState();
    mEd->mSavedSel->SaveSelection(mSel);
  }
}

nsAutoSelectionReset::~nsAutoSelectionReset()
{
  if (mSel && mEd->mSavedSel)   // mSel will be null if this was nested call
  {
    mEd->mSavedSel->RestoreSelection(mSel);
    delete mEd->mSavedSel;
    mEd->mSavedSel = nsnull;
  }
}


