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

#include "nsTextEditRules.h"
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsISelectionPrivate.h"
#include "nsFrameSelection.h"

// Test for distance between caret and text that will be deleted
nsresult
nsTextEditRules::CheckBidiLevelForDeletion(nsISelection         *aSelection,
                                           nsIDOMNode           *aSelNode, 
                                           PRInt32               aSelOffset, 
                                           nsIEditor::EDirection aAction,
                                           bool                 *aCancel)
{
  NS_ENSURE_ARG_POINTER(aCancel);
  *aCancel = PR_FALSE;

  nsCOMPtr<nsIPresShell> shell = mEditor->GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_NOT_INITIALIZED);
  
  nsPresContext *context = shell->GetPresContext();
  NS_ENSURE_TRUE(context, NS_ERROR_NULL_POINTER);
  
  if (!context->BidiEnabled())
    return NS_OK;
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aSelNode);
  NS_ENSURE_TRUE(content, NS_ERROR_NULL_POINTER);

  PRUint8 levelBefore;
  PRUint8 levelAfter;

  nsCOMPtr<nsISelectionPrivate> privateSelection(do_QueryInterface(aSelection));
  NS_ENSURE_TRUE(privateSelection, NS_ERROR_NULL_POINTER);
  
  nsRefPtr<nsFrameSelection> frameSelection;
  privateSelection->GetFrameSelection(getter_AddRefs(frameSelection));
  NS_ENSURE_TRUE(frameSelection, NS_ERROR_NULL_POINTER);
  
  nsPrevNextBidiLevels levels = frameSelection->
    GetPrevNextBidiLevels(content, aSelOffset, PR_TRUE);
    
  levelBefore = levels.mLevelBefore;
  levelAfter = levels.mLevelAfter;

  PRUint8 currentCaretLevel = frameSelection->GetCaretBidiLevel();

  PRUint8 levelOfDeletion;
  levelOfDeletion =
    (nsIEditor::eNext==aAction || nsIEditor::eNextWord==aAction) ?
    levelAfter : levelBefore;

  if (currentCaretLevel == levelOfDeletion)
    ; // perform the deletion
  else
  {
    if (mDeleteBidiImmediately || levelBefore == levelAfter)
      ; // perform the deletion
    else
      *aCancel = PR_TRUE;

    // Set the bidi level of the caret to that of the
    // character that will be (or would have been) deleted
    frameSelection->SetCaretBidiLevel(levelOfDeletion);
  }
  return NS_OK;
}

