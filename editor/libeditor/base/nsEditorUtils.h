/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#ifndef nsEditorUtils_h__
#define nsEditorUtils_h__


#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIDOMSelection.h"
#include "nsIEditor.h"

class nsAutoEditBatch
{
  private:
    nsCOMPtr<nsIEditor> mEd;
  public:
    nsAutoEditBatch( nsIEditor *aEd) : mEd(do_QueryInterface(aEd)) 
                   { if (mEd) mEd->BeginTransaction(); }
    ~nsAutoEditBatch() { if (mEd) mEd->EndTransaction(); }
};

class nsAutoSelectionReset
{
  private:
    /** ref-counted reference to the selection that we are supposed to restore */
    nsCOMPtr<nsIDOMSelection> mSel;

    /** PR_TRUE if this instance initialized itself correctly */
    PRBool mInitialized;

    nsCOMPtr<nsIDOMNode> mStartNode;
    nsCOMPtr<nsIDOMNode> mEndNode;
    PRInt32 mStartOffset;
    PRInt32 mEndOffset;

  public:
    /** constructor responsible for remembering all state needed to restore aSel */
    nsAutoSelectionReset(nsIDOMSelection *aSel);
    
    /** destructor restores mSel to its former state */
    ~nsAutoSelectionReset();
};


#endif // nsEditorUtils_h__
