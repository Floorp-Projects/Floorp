/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorCommands.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/FlushType.h"
#include "mozilla/TextEditor.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIClipboard.h"
#include "nsICommandParams.h"
#include "nsID.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIEditor.h"
#include "nsIEditorMailSupport.h"
#include "nsIPlaintextEditor.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsITransferable.h"
#include "nsString.h"
#include "nsAString.h"

class nsISupports;

#define STATE_ENABLED "state_enabled"
#define STATE_DATA "state_data"

namespace mozilla {

/******************************************************************************
 * mozilla::EditorCommandBase
 ******************************************************************************/

EditorCommandBase::EditorCommandBase()
{
}

NS_IMPL_ISUPPORTS(EditorCommandBase, nsIControllerCommand)

/******************************************************************************
 * mozilla::UndoCommand
 ******************************************************************************/

NS_IMETHODIMP
UndoCommand::IsCommandEnabled(const char* aCommandName,
                              nsISupports* aCommandRefCon,
                              bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    bool isEnabled, isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return editor->CanUndo(&isEnabled, aIsEnabled);
  }

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
UndoCommand::DoCommand(const char* aCommandName,
                       nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->Undo(1);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
UndoCommand::DoCommandParams(const char* aCommandName,
                             nsICommandParams* aParams,
                             nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
UndoCommand::GetCommandStateParams(const char* aCommandName,
                                   nsICommandParams* aParams,
                                   nsISupports* aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED, canUndo);
}

/******************************************************************************
 * mozilla::RedoCommand
 ******************************************************************************/

NS_IMETHODIMP
RedoCommand::IsCommandEnabled(const char* aCommandName,
                              nsISupports* aCommandRefCon,
                              bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    bool isEnabled, isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return editor->CanRedo(&isEnabled, aIsEnabled);
  }

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
RedoCommand::DoCommand(const char* aCommandName,
                       nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->Redo(1);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
RedoCommand::DoCommandParams(const char* aCommandName,
                             nsICommandParams* aParams,
                             nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
RedoCommand::GetCommandStateParams(const char* aCommandName,
                                   nsICommandParams* aParams,
                                   nsISupports* aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED, canUndo);
}

/******************************************************************************
 * mozilla::ClearUndoCommand
 ******************************************************************************/

NS_IMETHODIMP
ClearUndoCommand::IsCommandEnabled(const char* aCommandName,
                                   nsISupports* aCommandRefCon,
                                   bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->GetIsSelectionEditable(aIsEnabled);

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
ClearUndoCommand::DoCommand(const char* aCommandName,
                            nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_NOT_IMPLEMENTED);

  editor->EnableUndo(false); // Turning off undo clears undo/redo stacks.
  editor->EnableUndo(true);  // This re-enables undo/redo.

  return NS_OK;
}

NS_IMETHODIMP
ClearUndoCommand::DoCommandParams(const char* aCommandName,
                                  nsICommandParams* aParams,
                                  nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
ClearUndoCommand::GetCommandStateParams(const char* aCommandName,
                                        nsICommandParams* aParams,
                                        nsISupports* aCommandRefCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  bool enabled;
  nsresult rv = IsCommandEnabled(aCommandName, aCommandRefCon, &enabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->SetBooleanValue(STATE_ENABLED, enabled);
}

/******************************************************************************
 * mozilla::CutCommand
 ******************************************************************************/

NS_IMETHODIMP
CutCommand::IsCommandEnabled(const char* aCommandName,
                             nsISupports* aCommandRefCon,
                             bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    bool isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return editor->CanCut(aIsEnabled);
  }

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
CutCommand::DoCommand(const char* aCommandName,
                      nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->Cut();

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CutCommand::DoCommandParams(const char* aCommandName,
                            nsICommandParams* aParams,
                            nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
CutCommand::GetCommandStateParams(const char* aCommandName,
                                  nsICommandParams* aParams,
                                  nsISupports* aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED, canUndo);
}

/******************************************************************************
 * mozilla::CutOrDeleteCommand
 ******************************************************************************/

NS_IMETHODIMP
CutOrDeleteCommand::IsCommandEnabled(const char* aCommandName,
                                     nsISupports* aCommandRefCon,
                                     bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->GetIsSelectionEditable(aIsEnabled);

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
CutOrDeleteCommand::DoCommand(const char* aCommandName,
                              nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    nsCOMPtr<nsISelection> selection;
    nsresult rv = editor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(rv) && selection && selection->Collapsed()) {
      return editor->DeleteSelection(nsIEditor::eNext, nsIEditor::eStrip);
    }
    return editor->Cut();
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CutOrDeleteCommand::DoCommandParams(const char* aCommandName,
                                    nsICommandParams* aParams,
                                    nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
CutOrDeleteCommand::GetCommandStateParams(const char* aCommandName,
                                          nsICommandParams* aParams,
                                          nsISupports* aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED, canUndo);
}

/******************************************************************************
 * mozilla::CopyCommand
 ******************************************************************************/

NS_IMETHODIMP
CopyCommand::IsCommandEnabled(const char* aCommandName,
                              nsISupports* aCommandRefCon,
                              bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->CanCopy(aIsEnabled);

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
CopyCommand::DoCommand(const char* aCommandName,
                       nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->Copy();

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CopyCommand::DoCommandParams(const char* aCommandName,
                             nsICommandParams* aParams,
                             nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
CopyCommand::GetCommandStateParams(const char* aCommandName,
                                   nsICommandParams* aParams,
                                   nsISupports* aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED, canUndo);
}

/******************************************************************************
 * mozilla::CopyOrDeleteCommand
 ******************************************************************************/

NS_IMETHODIMP
CopyOrDeleteCommand::IsCommandEnabled(const char* aCommandName,
                                      nsISupports* aCommandRefCon,
                                      bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->GetIsSelectionEditable(aIsEnabled);

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
CopyOrDeleteCommand::DoCommand(const char* aCommandName,
                               nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    nsCOMPtr<nsISelection> selection;
    nsresult rv = editor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(rv) && selection && selection->Collapsed()) {
      return editor->DeleteSelection(nsIEditor::eNextWord, nsIEditor::eStrip);
    }
    return editor->Copy();
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CopyOrDeleteCommand::DoCommandParams(const char* aCommandName,
                                     nsICommandParams* aParams,
                                     nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
CopyOrDeleteCommand::GetCommandStateParams(const char* aCommandName,
                                           nsICommandParams* aParams,
                                           nsISupports* aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED, canUndo);
}

/******************************************************************************
 * mozilla::CopyAndCollapseToEndCommand
 ******************************************************************************/

NS_IMETHODIMP
CopyAndCollapseToEndCommand::IsCommandEnabled(const char* aCommandName,
                                              nsISupports* aCommandRefCon,
                                              bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->CanCopy(aIsEnabled);

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
CopyAndCollapseToEndCommand::DoCommand(const char* aCommandName,
                                       nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    nsresult rv = editor->Copy();
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsISelection> selection;
    rv = editor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(rv) && selection) {
      selection->CollapseToEnd();
    }
    return rv;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CopyAndCollapseToEndCommand::DoCommandParams(const char* aCommandName,
                                             nsICommandParams* aParams,
                                             nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
CopyAndCollapseToEndCommand::GetCommandStateParams(const char* aCommandName,
                                                   nsICommandParams* aParams,
                                                   nsISupports* aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED, canUndo);
}

/******************************************************************************
 * mozilla::PasteCommand
 ******************************************************************************/

NS_IMETHODIMP
PasteCommand::IsCommandEnabled(const char* aCommandName,
                               nsISupports* aCommandRefCon,
                               bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    bool isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return editor->CanPaste(nsIClipboard::kGlobalClipboard, aIsEnabled);
  }

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
PasteCommand::DoCommand(const char* aCommandName,
                        nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);

  return editor->Paste(nsIClipboard::kGlobalClipboard);
}

NS_IMETHODIMP
PasteCommand::DoCommandParams(const char* aCommandName,
                              nsICommandParams* aParams,
                              nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
PasteCommand::GetCommandStateParams(const char* aCommandName,
                                    nsICommandParams* aParams,
                                    nsISupports* aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED, canUndo);
}

/******************************************************************************
 * mozilla::PasteTransferableCommand
 ******************************************************************************/

NS_IMETHODIMP
PasteTransferableCommand::IsCommandEnabled(const char* aCommandName,
                                           nsISupports* aCommandRefCon,
                                           bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    bool isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return editor->CanPasteTransferable(nullptr, aIsEnabled);
  }

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
PasteTransferableCommand::DoCommand(const char* aCommandName,
                                    nsISupports* aCommandRefCon)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PasteTransferableCommand::DoCommandParams(const char* aCommandName,
                                          nsICommandParams* aParams,
                                          nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);

  nsCOMPtr<nsISupports> supports;
  aParams->GetISupportsValue("transferable", getter_AddRefs(supports));
  NS_ENSURE_TRUE(supports, NS_ERROR_FAILURE);

  nsCOMPtr<nsITransferable> trans = do_QueryInterface(supports);
  NS_ENSURE_TRUE(trans, NS_ERROR_FAILURE);

  return editor->PasteTransferable(trans);
}

NS_IMETHODIMP
PasteTransferableCommand::GetCommandStateParams(const char* aCommandName,
                                                nsICommandParams* aParams,
                                                nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);

  nsCOMPtr<nsITransferable> trans;

  nsCOMPtr<nsISupports> supports;
  aParams->GetISupportsValue("transferable", getter_AddRefs(supports));
  if (supports) {
    trans = do_QueryInterface(supports);
    NS_ENSURE_TRUE(trans, NS_ERROR_FAILURE);
  }

  bool canPaste;
  nsresult rv = editor->CanPasteTransferable(trans, &canPaste);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->SetBooleanValue(STATE_ENABLED, canPaste);
}

/******************************************************************************
 * mozilla::SwitchTextDirectionCommand
 ******************************************************************************/

NS_IMETHODIMP
SwitchTextDirectionCommand::IsCommandEnabled(const char* aCommandName,
                                             nsISupports* aCommandRefCon,
                                             bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->GetIsSelectionEditable(aIsEnabled);

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
SwitchTextDirectionCommand::DoCommand(const char* aCommandName,
                                      nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);

  return editor->SwitchTextDirection();
}

NS_IMETHODIMP
SwitchTextDirectionCommand::DoCommandParams(const char* aCommandName,
                                            nsICommandParams* aParams,
                                            nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
SwitchTextDirectionCommand::GetCommandStateParams(const char* aCommandName,
                                                  nsICommandParams* aParams,
                                                  nsISupports* aCommandRefCon)
{
  bool canSwitchTextDirection = true;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canSwitchTextDirection);
  return aParams->SetBooleanValue(STATE_ENABLED, canSwitchTextDirection);
}

/******************************************************************************
 * mozilla::DeleteCommand
 ******************************************************************************/

NS_IMETHODIMP
DeleteCommand::IsCommandEnabled(const char* aCommandName,
                                nsISupports* aCommandRefCon,
                                bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  *aIsEnabled = false;

  if (!editor) {
    return NS_OK;
  }

  // We can generally delete whenever the selection is editable.  However,
  // cmd_delete doesn't make sense if the selection is collapsed because it's
  // directionless, which is the same condition under which we can't cut.
  nsresult rv = editor->GetIsSelectionEditable(aIsEnabled);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nsCRT::strcmp("cmd_delete", aCommandName) && *aIsEnabled) {
    rv = editor->CanDelete(aIsEnabled);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
DeleteCommand::DoCommand(const char* aCommandName,
                         nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);

  nsIEditor::EDirection deleteDir = nsIEditor::eNone;

  if (!nsCRT::strcmp("cmd_delete", aCommandName)) {
    // Really this should probably be eNone, but it only makes a difference if
    // the selection is collapsed, and then this command is disabled.  So let's
    // keep it as it always was to avoid breaking things.
    deleteDir = nsIEditor::ePrevious;
  } else if (!nsCRT::strcmp("cmd_deleteCharForward", aCommandName)) {
    deleteDir = nsIEditor::eNext;
  } else if (!nsCRT::strcmp("cmd_deleteCharBackward", aCommandName)) {
    deleteDir = nsIEditor::ePrevious;
  } else if (!nsCRT::strcmp("cmd_deleteWordBackward", aCommandName)) {
    deleteDir = nsIEditor::ePreviousWord;
  } else if (!nsCRT::strcmp("cmd_deleteWordForward", aCommandName)) {
    deleteDir = nsIEditor::eNextWord;
  } else if (!nsCRT::strcmp("cmd_deleteToBeginningOfLine", aCommandName)) {
    deleteDir = nsIEditor::eToBeginningOfLine;
  } else if (!nsCRT::strcmp("cmd_deleteToEndOfLine", aCommandName)) {
    deleteDir = nsIEditor::eToEndOfLine;
  } else {
    MOZ_CRASH("Unrecognized nsDeleteCommand");
  }

  return editor->DeleteSelection(deleteDir, nsIEditor::eStrip);
}

NS_IMETHODIMP
DeleteCommand::DoCommandParams(const char* aCommandName,
                               nsICommandParams* aParams,
                               nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
DeleteCommand::GetCommandStateParams(const char* aCommandName,
                                     nsICommandParams* aParams,
                                     nsISupports* aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED, canUndo);
}

/******************************************************************************
 * mozilla::SelectAllCommand
 ******************************************************************************/

NS_IMETHODIMP
SelectAllCommand::IsCommandEnabled(const char* aCommandName,
                                   nsISupports* aCommandRefCon,
                                   bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);

  nsresult rv = NS_OK;
  // You can always select all, unless the selection is editable,
  // and the editable region is empty!
  *aIsEnabled = true;
  bool docIsEmpty;

  // you can select all if there is an editor which is non-empty
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    rv = editor->GetDocumentIsEmpty(&docIsEmpty);
    NS_ENSURE_SUCCESS(rv, rv);
    *aIsEnabled = !docIsEmpty;
  }

  return rv;
}

NS_IMETHODIMP
SelectAllCommand::DoCommand(const char* aCommandName,
                            nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->SelectAll();

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SelectAllCommand::DoCommandParams(const char* aCommandName,
                                  nsICommandParams* aParams,
                                  nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
SelectAllCommand::GetCommandStateParams(const char* aCommandName,
                                        nsICommandParams* aParams,
                                        nsISupports* aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED, canUndo);
}

/******************************************************************************
 * mozilla::SelectionMoveCommands
 ******************************************************************************/

NS_IMETHODIMP
SelectionMoveCommands::IsCommandEnabled(const char* aCommandName,
                                        nsISupports* aCommandRefCon,
                                        bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->GetIsSelectionEditable(aIsEnabled);

  *aIsEnabled = false;
  return NS_OK;
}

static const struct ScrollCommand {
  const char *reverseScroll;
  const char *forwardScroll;
  nsresult (NS_STDCALL nsISelectionController::*scroll)(bool);
} scrollCommands[] = {
  { "cmd_scrollTop", "cmd_scrollBottom",
    &nsISelectionController::CompleteScroll },
  { "cmd_scrollPageUp", "cmd_scrollPageDown",
    &nsISelectionController::ScrollPage },
  { "cmd_scrollLineUp", "cmd_scrollLineDown",
    &nsISelectionController::ScrollLine }
};

static const struct MoveCommand {
  const char *reverseMove;
  const char *forwardMove;
  const char *reverseSelect;
  const char *forwardSelect;
  nsresult (NS_STDCALL nsISelectionController::*move)(bool, bool);
} moveCommands[] = {
  { "cmd_charPrevious", "cmd_charNext",
    "cmd_selectCharPrevious", "cmd_selectCharNext",
    &nsISelectionController::CharacterMove },
  { "cmd_linePrevious", "cmd_lineNext",
    "cmd_selectLinePrevious", "cmd_selectLineNext",
    &nsISelectionController::LineMove },
  { "cmd_wordPrevious", "cmd_wordNext",
    "cmd_selectWordPrevious", "cmd_selectWordNext",
    &nsISelectionController::WordMove },
  { "cmd_beginLine", "cmd_endLine",
    "cmd_selectBeginLine", "cmd_selectEndLine",
    &nsISelectionController::IntraLineMove },
  { "cmd_movePageUp", "cmd_movePageDown",
    "cmd_selectPageUp", "cmd_selectPageDown",
    &nsISelectionController::PageMove },
  { "cmd_moveTop", "cmd_moveBottom",
    "cmd_selectTop", "cmd_selectBottom",
    &nsISelectionController::CompleteMove }
};

static const struct PhysicalCommand {
  const char *move;
  const char *select;
  int16_t direction;
  int16_t amount;
} physicalCommands[] = {
  { "cmd_moveLeft", "cmd_selectLeft",
    nsISelectionController::MOVE_LEFT, 0 },
  { "cmd_moveRight", "cmd_selectRight",
    nsISelectionController::MOVE_RIGHT, 0 },
  { "cmd_moveUp", "cmd_selectUp",
    nsISelectionController::MOVE_UP, 0 },
  { "cmd_moveDown", "cmd_selectDown",
    nsISelectionController::MOVE_DOWN, 0 },
  { "cmd_moveLeft2", "cmd_selectLeft2",
    nsISelectionController::MOVE_LEFT, 1 },
  { "cmd_moveRight2", "cmd_selectRight2",
    nsISelectionController::MOVE_RIGHT, 1 },
  { "cmd_moveUp2", "cmd_selectUp2",
    nsISelectionController::MOVE_UP, 1 },
  { "cmd_moveDown2", "cmd_selectDown2",
    nsISelectionController::MOVE_DOWN, 1 }
};

NS_IMETHODIMP
SelectionMoveCommands::DoCommand(const char* aCommandName,
                                 nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDocument> domDoc;
  editor->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (doc) {
    // Most of the commands below (possibly all of them) need layout to
    // be up to date.
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  nsCOMPtr<nsISelectionController> selCont;
  nsresult rv = editor->GetSelectionController(getter_AddRefs(selCont));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(selCont, NS_ERROR_FAILURE);

  // scroll commands
  for (size_t i = 0; i < mozilla::ArrayLength(scrollCommands); i++) {
    const ScrollCommand &cmd = scrollCommands[i];
    if (!nsCRT::strcmp(aCommandName, cmd.reverseScroll)) {
      return (selCont->*(cmd.scroll))(false);
    } else if (!nsCRT::strcmp(aCommandName, cmd.forwardScroll)) {
      return (selCont->*(cmd.scroll))(true);
    }
  }

  // caret movement/selection commands
  for (size_t i = 0; i < mozilla::ArrayLength(moveCommands); i++) {
    const MoveCommand &cmd = moveCommands[i];
    if (!nsCRT::strcmp(aCommandName, cmd.reverseMove)) {
      return (selCont->*(cmd.move))(false, false);
    } else if (!nsCRT::strcmp(aCommandName, cmd.forwardMove)) {
      return (selCont->*(cmd.move))(true, false);
    } else if (!nsCRT::strcmp(aCommandName, cmd.reverseSelect)) {
      return (selCont->*(cmd.move))(false, true);
    } else if (!nsCRT::strcmp(aCommandName, cmd.forwardSelect)) {
      return (selCont->*(cmd.move))(true, true);
    }
  }

  // physical-direction movement/selection
  for (size_t i = 0; i < mozilla::ArrayLength(physicalCommands); i++) {
    const PhysicalCommand &cmd = physicalCommands[i];
    if (!nsCRT::strcmp(aCommandName, cmd.move)) {
      return selCont->PhysicalMove(cmd.direction, cmd.amount, false);
    } else if (!nsCRT::strcmp(aCommandName, cmd.select)) {
      return selCont->PhysicalMove(cmd.direction, cmd.amount, true);
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SelectionMoveCommands::DoCommandParams(const char* aCommandName,
                                       nsICommandParams* aParams,
                                       nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
SelectionMoveCommands::GetCommandStateParams(const char* aCommandName,
                                             nsICommandParams* aParams,
                                             nsISupports* aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED, canUndo);
}

/******************************************************************************
 * mozilla::InsertPlaintextCommand
 ******************************************************************************/

NS_IMETHODIMP
InsertPlaintextCommand::IsCommandEnabled(const char* aCommandName,
                                         nsISupports* aCommandRefCon,
                                         bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->GetIsSelectionEditable(aIsEnabled);

  *aIsEnabled = false;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InsertPlaintextCommand::DoCommand(const char* aCommandName,
                                  nsISupports* aCommandRefCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InsertPlaintextCommand::DoCommandParams(const char* aCommandName,
                                        nsICommandParams* aParams,
                                        nsISupports* aCommandRefCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  nsCOMPtr<nsIPlaintextEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_NOT_IMPLEMENTED);

  // Get text to insert from command params
  nsAutoString text;
  nsresult rv = aParams->GetStringValue(STATE_DATA, text);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!text.IsEmpty())
    return editor->InsertText(text);

  return NS_OK;
}

NS_IMETHODIMP
InsertPlaintextCommand::GetCommandStateParams(const char* aCommandName,
                                              nsICommandParams* aParams,
                                              nsISupports* aCommandRefCon)
{
  if (NS_WARN_IF(!aParams)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool aIsEnabled = false;
  IsCommandEnabled(aCommandName, aCommandRefCon, &aIsEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED, aIsEnabled);
}

/******************************************************************************
 * mozilla::InsertParagraphCommand
 ******************************************************************************/

NS_IMETHODIMP
InsertParagraphCommand::IsCommandEnabled(const char* aCommandName,
                                         nsISupports* aCommandRefCon,
                                         bool* aIsEnabled)
{
  if (NS_WARN_IF(!aIsEnabled)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (NS_WARN_IF(!editor)) {
    *aIsEnabled = false;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return editor->GetIsSelectionEditable(aIsEnabled);
}

NS_IMETHODIMP
InsertParagraphCommand::DoCommand(const char* aCommandName,
                                  nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIPlaintextEditor> editor = do_QueryInterface(aCommandRefCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  TextEditor* textEditor = static_cast<TextEditor*>(editor.get());

  return textEditor->TypedText(EmptyString(), TextEditor::eTypedBreak);
}

NS_IMETHODIMP
InsertParagraphCommand::DoCommandParams(const char* aCommandName,
                                        nsICommandParams* aParams,
                                        nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
InsertParagraphCommand::GetCommandStateParams(const char* aCommandName,
                                              nsICommandParams* aParams,
                                              nsISupports* aCommandRefCon)
{
  if (NS_WARN_IF(!aParams)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool aIsEnabled = false;
  IsCommandEnabled(aCommandName, aCommandRefCon, &aIsEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED, aIsEnabled);
}

/******************************************************************************
 * mozilla::InsertLineBreakCommand
 ******************************************************************************/

NS_IMETHODIMP
InsertLineBreakCommand::IsCommandEnabled(const char* aCommandName,
                                         nsISupports* aCommandRefCon,
                                         bool* aIsEnabled)
{
  if (NS_WARN_IF(!aIsEnabled)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (NS_WARN_IF(!editor)) {
    *aIsEnabled = false;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return editor->GetIsSelectionEditable(aIsEnabled);
}

NS_IMETHODIMP
InsertLineBreakCommand::DoCommand(const char* aCommandName,
                                  nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIPlaintextEditor> editor = do_QueryInterface(aCommandRefCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  TextEditor* textEditor = static_cast<TextEditor*>(editor.get());

  return textEditor->TypedText(EmptyString(), TextEditor::eTypedBR);
}

NS_IMETHODIMP
InsertLineBreakCommand::DoCommandParams(const char* aCommandName,
                                        nsICommandParams* aParams,
                                        nsISupports* aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
InsertLineBreakCommand::GetCommandStateParams(const char* aCommandName,
                                              nsICommandParams* aParams,
                                              nsISupports* aCommandRefCon)
{
  if (NS_WARN_IF(!aParams)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool aIsEnabled = false;
  IsCommandEnabled(aCommandName, aCommandRefCon, &aIsEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED, aIsEnabled);
}

/******************************************************************************
 * mozilla::PasteQuotationCommand
 ******************************************************************************/

NS_IMETHODIMP
PasteQuotationCommand::IsCommandEnabled(const char* aCommandName,
                                        nsISupports* aCommandRefCon,
                                        bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(aCommandRefCon);
  if (editor && mailEditor) {
    uint32_t flags;
    editor->GetFlags(&flags);
    if (!(flags & nsIPlaintextEditor::eEditorSingleLineMask))
      return editor->CanPaste(nsIClipboard::kGlobalClipboard, aIsEnabled);
  }

  *aIsEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
PasteQuotationCommand::DoCommand(const char* aCommandName,
                                 nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(aCommandRefCon);
  if (mailEditor)
    return mailEditor->PasteAsQuotation(nsIClipboard::kGlobalClipboard);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PasteQuotationCommand::DoCommandParams(const char* aCommandName,
                                       nsICommandParams* aParams,
                                       nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(aCommandRefCon);
  if (mailEditor)
    return mailEditor->PasteAsQuotation(nsIClipboard::kGlobalClipboard);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PasteQuotationCommand::GetCommandStateParams(const char* aCommandName,
                                             nsICommandParams* aParams,
                                             nsISupports* aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    bool enabled = false;
    editor->CanPaste(nsIClipboard::kGlobalClipboard, &enabled);
    aParams->SetBooleanValue(STATE_ENABLED, enabled);
  }

  return NS_OK;
}

} // namespace mozilla
