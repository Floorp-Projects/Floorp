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

#include "nsTextEditRules.h"
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIFrame.h"

// Test for distance between caret and text that will be deleted
nsresult
nsTextEditRules::CheckBidiLevelForDeletion(nsIDOMNode           *aSelNode, 
                                           PRInt32               aSelOffset, 
                                           nsIEditor::EDirection aAction,
                                           PRBool               *aCancel)
{
  NS_ENSURE_ARG_POINTER(aCancel);
  *aCancel = PR_FALSE;
  nsresult res = NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  res = mEditor->GetPresShell(getter_AddRefs(shell));
  if (NS_FAILED(res))
    return res;
  if (!shell)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIPresContext> context;
  res = shell->GetPresContext(getter_AddRefs(context));
  if (NS_FAILED(res))
    return res;
  if (!context)
    return NS_ERROR_NULL_POINTER;
  
  PRBool bidiEnabled;
  context->GetBidiEnabled(&bidiEnabled);
  if (!bidiEnabled)
    return NS_OK;
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aSelNode);
  if (!content)
    return NS_ERROR_NULL_POINTER;
  
  nsIFrame *primaryFrame;
  nsIFrame *frameBefore;
  nsIFrame *frameAfter;
  PRInt32 frameOffset;

  res = shell->GetPrimaryFrameFor(content, &primaryFrame);
  if (NS_FAILED(res))
    return res;
  if (!primaryFrame)
    return NS_ERROR_NULL_POINTER;
  
  res = primaryFrame->GetChildFrameContainingOffset(aSelOffset, PR_FALSE, &frameOffset, &frameBefore);
  if (NS_FAILED(res))
    return res;
  if (!frameBefore)
    return NS_ERROR_NULL_POINTER;
  
  PRInt32 start, end;
  PRUint8 currentCursorLevel;
  PRUint8 levelAfter;
  PRUint8 levelBefore;
  PRUint8 levelOfDeletion;
  nsCOMPtr<nsIAtom> embeddingLevel = getter_AddRefs(NS_NewAtom("EmbeddingLevel")); 
  nsCOMPtr<nsIAtom> baseLevel = getter_AddRefs(NS_NewAtom("BaseLevel"));

  // Get the bidi level of the frame before the caret
  res = frameBefore->GetBidiProperty(context, embeddingLevel, (void**)&levelBefore,sizeof(PRUint8));
  if (NS_FAILED(res))
    return res;

  // If the caret is at the end of the frame, get the bidi level of the
  // frame after the caret
  frameBefore->GetOffsets(start, end);
  if (aSelOffset == end
     || aSelOffset == -1)
  {
    res = primaryFrame->GetChildFrameContainingOffset(aSelOffset, PR_TRUE, &frameOffset, &frameAfter);
    if (NS_FAILED(res))
      return res;
    if (!frameAfter)
      return NS_ERROR_NULL_POINTER;
    
    if (frameBefore==frameAfter)
    {
      // there was no frameAfter, i.e. the caret is at the end of the
      // document -- use the base paragraph level
      res = frameBefore->GetBidiProperty(context, baseLevel, (void**)&levelAfter,sizeof(PRUint8));
      if (NS_FAILED(res))
        return res;
    }
    else
    {
      res = frameAfter->GetBidiProperty(context, embeddingLevel, (void**)&levelAfter,sizeof(PRUint8));
      if (NS_FAILED(res))
        return res;
    }
  }
  else
  {
    levelAfter = levelBefore;
  }
  res = shell->GetCursorBidiLevel(&currentCursorLevel);
  if (NS_FAILED(res))
    return res;
  levelOfDeletion = (nsIEditor::eNext==aAction) ? levelAfter : levelBefore;

  if (currentCursorLevel == levelOfDeletion)
    ; // perform the deletion
  else
  {
    if ((levelBefore==levelAfter)
        && (levelBefore & 1) == (currentCursorLevel & 1))
      ; // perform the deletion
    else
      *aCancel = PR_TRUE;

    // Set the bidi level of the caret to that of the
    // character that will be (or would have been) deleted
    res = shell->SetCursorBidiLevel(levelOfDeletion);
    if (NS_FAILED(res))
      return res;
  }
  return NS_OK;
}

