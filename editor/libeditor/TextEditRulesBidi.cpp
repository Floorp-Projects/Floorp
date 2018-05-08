/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextEditRules.h"

#include "mozilla/TextEditor.h"
#include "mozilla/dom/Selection.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFrameSelection.h"
#include "nsIContent.h"
#include "nsIEditor.h"
#include "nsIPresShell.h"
#include "nsISupportsImpl.h"
#include "nsPresContext.h"
#include "nscore.h"

namespace mozilla {

using namespace dom;

// Test for distance between caret and text that will be deleted
nsresult
TextEditRules::CheckBidiLevelForDeletion(
                 Selection* aSelection,
                 const EditorRawDOMPoint& aSelectionPoint,
                 nsIEditor::EDirection aAction,
                 bool* aCancel)
{
  NS_ENSURE_ARG_POINTER(aCancel);
  *aCancel = false;

  nsCOMPtr<nsIPresShell> shell = mTextEditor->GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_NOT_INITIALIZED);

  nsPresContext *context = shell->GetPresContext();
  NS_ENSURE_TRUE(context, NS_ERROR_NULL_POINTER);

  if (!context->BidiEnabled()) {
    return NS_OK;
  }

  if (!aSelectionPoint.GetContainerAsContent()) {
    return NS_ERROR_NULL_POINTER;
  }

  nsBidiLevel levelBefore;
  nsBidiLevel levelAfter;
  RefPtr<nsFrameSelection> frameSelection = aSelection->GetFrameSelection();
  NS_ENSURE_TRUE(frameSelection, NS_ERROR_NULL_POINTER);

  nsPrevNextBidiLevels levels = frameSelection->
    GetPrevNextBidiLevels(aSelectionPoint.GetContainerAsContent(),
                          aSelectionPoint.Offset(), true);

  levelBefore = levels.mLevelBefore;
  levelAfter = levels.mLevelAfter;

  nsBidiLevel currentCaretLevel = frameSelection->GetCaretBidiLevel();

  nsBidiLevel levelOfDeletion;
  levelOfDeletion =
    (nsIEditor::eNext==aAction || nsIEditor::eNextWord==aAction) ?
    levelAfter : levelBefore;

  if (currentCaretLevel == levelOfDeletion) {
    return NS_OK; // perform the deletion
  }

  if (!mDeleteBidiImmediately && levelBefore != levelAfter) {
    *aCancel = true;
  }

  // Set the bidi level of the caret to that of the
  // character that will be (or would have been) deleted
  frameSelection->SetCaretBidiLevel(levelOfDeletion);
  return NS_OK;
}

void
TextEditRules::UndefineCaretBidiLevel(Selection* aSelection)
{
  /**
   * After inserting text the caret Bidi level must be set to the level of the
   * inserted text.This is difficult, because we cannot know what the level is
   * until after the Bidi algorithm is applied to the whole paragraph.
   *
   * So we set the caret Bidi level to UNDEFINED here, and the caret code will
   * set it correctly later
   */
  RefPtr<nsFrameSelection> frameSelection = aSelection->GetFrameSelection();
  if (frameSelection) {
    frameSelection->UndefineCaretBidiLevel();
  }
}

} // namespace mozilla
