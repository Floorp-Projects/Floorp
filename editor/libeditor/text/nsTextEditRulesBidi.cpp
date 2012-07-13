/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFrameSelection.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIEditor.h"
#include "nsIPresShell.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISupportsImpl.h"
#include "nsPlaintextEditor.h"
#include "nsPresContext.h"
#include "nsTextEditRules.h"
#include "nscore.h"
#include "prtypes.h"

// Test for distance between caret and text that will be deleted
nsresult
nsTextEditRules::CheckBidiLevelForDeletion(nsISelection         *aSelection,
                                           nsIDOMNode           *aSelNode, 
                                           PRInt32               aSelOffset, 
                                           nsIEditor::EDirection aAction,
                                           bool                 *aCancel)
{
  NS_ENSURE_ARG_POINTER(aCancel);
  *aCancel = false;

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
    GetPrevNextBidiLevels(content, aSelOffset, true);
    
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
      *aCancel = true;

    // Set the bidi level of the caret to that of the
    // character that will be (or would have been) deleted
    frameSelection->SetCaretBidiLevel(levelOfDeletion);
  }
  return NS_OK;
}

