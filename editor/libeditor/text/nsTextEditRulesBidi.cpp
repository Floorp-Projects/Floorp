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

  nsCOMPtr<nsIPresShell> shell;
  nsresult res = mEditor->GetPresShell(getter_AddRefs(shell));
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
  
  if (!context->BidiEnabled())
    return NS_OK;
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aSelNode);
  if (!content)
    return NS_ERROR_NULL_POINTER;
  
  nsIFrame *primaryFrame;
  res = shell->GetPrimaryFrameFor(content, &primaryFrame);
  if (NS_FAILED(res))
    return res;
  if (!primaryFrame)
    return NS_ERROR_NULL_POINTER;
  
  nsIFrame *frameBefore;
  nsIFrame *frameAfter;
  PRInt32 frameOffset;

  res = primaryFrame->GetChildFrameContainingOffset(aSelOffset, PR_FALSE, &frameOffset, &frameBefore);
  if (NS_FAILED(res))
    return res;
  if (!frameBefore)
    return NS_ERROR_NULL_POINTER;
  
  PRUint8 levelAfter;
  nsCOMPtr<nsIAtom> embeddingLevel = do_GetAtom("EmbeddingLevel");

  // Get the bidi level of the frame before the caret
  PRUint8 levelBefore =
    NS_PTR_TO_INT32(frameBefore->GetPropertyExternal(embeddingLevel, nsnull));

  // If the caret is at the end of the frame, get the bidi level of the
  // frame after the caret
  PRInt32 start, end;
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
      nsCOMPtr<nsIAtom> baseLevel = do_GetAtom("BaseLevel");
      levelAfter =
        NS_PTR_TO_INT32(frameBefore->GetPropertyExternal(baseLevel, nsnull));
    }
    else
    {
      levelAfter =
        NS_PTR_TO_INT32(frameAfter->GetPropertyExternal(embeddingLevel, nsnull));
    }
  }
  else
  {
    levelAfter = levelBefore;
  }
  PRUint8 currentCursorLevel;
  res = shell->GetCaretBidiLevel(&currentCursorLevel);
  if (NS_FAILED(res))
    return res;

  PRUint8 levelOfDeletion;
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
    res = shell->SetCaretBidiLevel(levelOfDeletion);
    if (NS_FAILED(res))
      return res;
  }
  return NS_OK;
}

