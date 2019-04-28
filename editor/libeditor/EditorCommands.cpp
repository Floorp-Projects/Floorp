/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorCommands.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/FlushType.h"
#include "mozilla/TextEditor.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Selection.h"
#include "nsCommandParams.h"
#include "nsIClipboard.h"
#include "nsIEditingSession.h"
#include "nsISelectionController.h"
#include "nsITransferable.h"
#include "nsString.h"
#include "nsAString.h"

class nsISupports;

#define STATE_ENABLED "state_enabled"
#define STATE_DATA "state_data"

namespace mozilla {

/******************************************************************************
 * mozilla::EditorCommand
 ******************************************************************************/

NS_IMPL_ISUPPORTS(EditorCommand, nsIControllerCommand)

NS_IMETHODIMP
EditorCommand::IsCommandEnabled(const char* aCommandName,
                                nsISupports* aCommandRefCon, bool* aIsEnabled) {
  if (NS_WARN_IF(!aCommandName) || NS_WARN_IF(!aIsEnabled)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  TextEditor* textEditor = editor ? editor->AsTextEditor() : nullptr;
  *aIsEnabled = IsCommandEnabled(aCommandName, MOZ_KnownLive(textEditor));
  return NS_OK;
}

NS_IMETHODIMP
EditorCommand::DoCommand(const char* aCommandName,
                         nsISupports* aCommandRefCon) {
  if (NS_WARN_IF(!aCommandName) || NS_WARN_IF(!aCommandRefCon)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv = DoCommand(aCommandName, MOZ_KnownLive(*editor->AsTextEditor()));
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "Failed to do command from nsIControllerCommand::DoCommand()");
  return rv;
}

NS_IMETHODIMP
EditorCommand::DoCommandParams(const char* aCommandName,
                               nsICommandParams* aParams,
                               nsISupports* aCommandRefCon) {
  if (NS_WARN_IF(!aCommandName) || NS_WARN_IF(!aCommandRefCon)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv =
      DoCommandParams(aCommandName, MOZ_KnownLive(aParams->AsCommandParams()),
                      MOZ_KnownLive(*editor->AsTextEditor()));
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "Failed to do command from nsIControllerCommand::DoCommandParams()");
  return rv;
}

NS_IMETHODIMP
EditorCommand::GetCommandStateParams(const char* aCommandName,
                                     nsICommandParams* aParams,
                                     nsISupports* aCommandRefCon) {
  if (NS_WARN_IF(!aCommandName) || NS_WARN_IF(!aParams)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    return GetCommandStateParams(
        aCommandName, MOZ_KnownLive(*aParams->AsCommandParams()),
        MOZ_KnownLive(editor->AsTextEditor()), nullptr);
  }
  nsCOMPtr<nsIEditingSession> editingSession =
      do_QueryInterface(aCommandRefCon);
  if (editingSession) {
    return GetCommandStateParams(aCommandName,
                                 MOZ_KnownLive(*aParams->AsCommandParams()),
                                 nullptr, editingSession);
  }
  return GetCommandStateParams(aCommandName,
                               MOZ_KnownLive(*aParams->AsCommandParams()),
                               nullptr, nullptr);
}

/******************************************************************************
 * mozilla::UndoCommand
 ******************************************************************************/

StaticRefPtr<UndoCommand> UndoCommand::sInstance;

bool UndoCommand::IsCommandEnabled(const char* aCommandName,
                                   TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable() && aTextEditor->CanUndo();
}

nsresult UndoCommand::DoCommand(const char* aCommandName,
                                TextEditor& aTextEditor) const {
  return aTextEditor.Undo(1);
}

nsresult UndoCommand::DoCommandParams(const char* aCommandName,
                                      nsCommandParams* aParams,
                                      TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult UndoCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::RedoCommand
 ******************************************************************************/

StaticRefPtr<RedoCommand> RedoCommand::sInstance;

bool RedoCommand::IsCommandEnabled(const char* aCommandName,
                                   TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable() && aTextEditor->CanRedo();
}

nsresult RedoCommand::DoCommand(const char* aCommandName,
                                TextEditor& aTextEditor) const {
  return aTextEditor.Redo(1);
}

nsresult RedoCommand::DoCommandParams(const char* aCommandName,
                                      nsCommandParams* aParams,
                                      TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult RedoCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::CutCommand
 ******************************************************************************/

StaticRefPtr<CutCommand> CutCommand::sInstance;

bool CutCommand::IsCommandEnabled(const char* aCommandName,
                                  TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable() && aTextEditor->CanCut();
}

nsresult CutCommand::DoCommand(const char* aCommandName,
                               TextEditor& aTextEditor) const {
  return aTextEditor.Cut();
}

nsresult CutCommand::DoCommandParams(const char* aCommandName,
                                     nsCommandParams* aParams,
                                     TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult CutCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::CutOrDeleteCommand
 ******************************************************************************/

StaticRefPtr<CutOrDeleteCommand> CutOrDeleteCommand::sInstance;

bool CutOrDeleteCommand::IsCommandEnabled(const char* aCommandName,
                                          TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult CutOrDeleteCommand::DoCommand(const char* aCommandName,
                                       TextEditor& aTextEditor) const {
  dom::Selection* selection = aTextEditor.GetSelection();
  if (selection && selection->IsCollapsed()) {
    nsresult rv = aTextEditor.DeleteSelectionAsAction(nsIEditor::eNext,
                                                      nsIEditor::eStrip);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }
  return aTextEditor.Cut();
}

nsresult CutOrDeleteCommand::DoCommandParams(const char* aCommandName,
                                             nsCommandParams* aParams,
                                             TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult CutOrDeleteCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::CopyCommand
 ******************************************************************************/

StaticRefPtr<CopyCommand> CopyCommand::sInstance;

bool CopyCommand::IsCommandEnabled(const char* aCommandName,
                                   TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->CanCopy();
}

nsresult CopyCommand::DoCommand(const char* aCommandName,
                                TextEditor& aTextEditor) const {
  return aTextEditor.Copy();
}

nsresult CopyCommand::DoCommandParams(const char* aCommandName,
                                      nsCommandParams* aParams,
                                      TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult CopyCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::CopyOrDeleteCommand
 ******************************************************************************/

StaticRefPtr<CopyOrDeleteCommand> CopyOrDeleteCommand::sInstance;

bool CopyOrDeleteCommand::IsCommandEnabled(const char* aCommandName,
                                           TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult CopyOrDeleteCommand::DoCommand(const char* aCommandName,
                                        TextEditor& aTextEditor) const {
  dom::Selection* selection = aTextEditor.GetSelection();
  if (selection && selection->IsCollapsed()) {
    nsresult rv = aTextEditor.DeleteSelectionAsAction(nsIEditor::eNextWord,
                                                      nsIEditor::eStrip);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }
  return aTextEditor.Copy();
}

nsresult CopyOrDeleteCommand::DoCommandParams(const char* aCommandName,
                                              nsCommandParams* aParams,
                                              TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult CopyOrDeleteCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::PasteCommand
 ******************************************************************************/

StaticRefPtr<PasteCommand> PasteCommand::sInstance;

bool PasteCommand::IsCommandEnabled(const char* aCommandName,
                                    TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable() &&
         aTextEditor->CanPaste(nsIClipboard::kGlobalClipboard);
}

nsresult PasteCommand::DoCommand(const char* aCommandName,
                                 TextEditor& aTextEditor) const {
  return aTextEditor.PasteAsAction(nsIClipboard::kGlobalClipboard, true);
}

nsresult PasteCommand::DoCommandParams(const char* aCommandName,
                                       nsCommandParams* aParams,
                                       TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult PasteCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::PasteTransferableCommand
 ******************************************************************************/

StaticRefPtr<PasteTransferableCommand> PasteTransferableCommand::sInstance;

bool PasteTransferableCommand::IsCommandEnabled(const char* aCommandName,
                                                TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable() &&
         aTextEditor->CanPasteTransferable(nullptr);
}

nsresult PasteTransferableCommand::DoCommand(const char* aCommandName,
                                             TextEditor& aTextEditor) const {
  return NS_ERROR_FAILURE;
}

nsresult PasteTransferableCommand::DoCommandParams(
    const char* aCommandName, nsCommandParams* aParams,
    TextEditor& aTextEditor) const {
  if (NS_WARN_IF(!aParams)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsISupports> supports = aParams->GetISupports("transferable");
  if (NS_WARN_IF(!supports)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsITransferable> trans = do_QueryInterface(supports);
  if (NS_WARN_IF(!trans)) {
    return NS_ERROR_FAILURE;
  }

  // We know textEditor is known-live here because we are holding a ref to it
  // via "editor".
  nsresult rv = aTextEditor.PasteTransferable(trans);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult PasteTransferableCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  if (NS_WARN_IF(!aTextEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsISupports> supports = aParams.GetISupports("transferable");
  if (NS_WARN_IF(!supports)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsITransferable> trans;
  trans = do_QueryInterface(supports);
  if (NS_WARN_IF(!trans)) {
    return NS_ERROR_FAILURE;
  }

  return aParams.SetBool(STATE_ENABLED,
                         aTextEditor->CanPasteTransferable(trans));
}

/******************************************************************************
 * mozilla::SwitchTextDirectionCommand
 ******************************************************************************/

StaticRefPtr<SwitchTextDirectionCommand> SwitchTextDirectionCommand::sInstance;

bool SwitchTextDirectionCommand::IsCommandEnabled(
    const char* aCommandName, TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult SwitchTextDirectionCommand::DoCommand(const char* aCommandName,
                                               TextEditor& aTextEditor) const {
  return aTextEditor.ToggleTextDirection();
}

nsresult SwitchTextDirectionCommand::DoCommandParams(
    const char* aCommandName, nsCommandParams* aParams,
    TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult SwitchTextDirectionCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::DeleteCommand
 ******************************************************************************/

StaticRefPtr<DeleteCommand> DeleteCommand::sInstance;

bool DeleteCommand::IsCommandEnabled(const char* aCommandName,
                                     TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  // We can generally delete whenever the selection is editable.  However,
  // cmd_delete doesn't make sense if the selection is collapsed because it's
  // directionless, which is the same condition under which we can't cut.
  bool isEnabled = aTextEditor->IsSelectionEditable();

  if (!nsCRT::strcmp("cmd_delete", aCommandName) && isEnabled) {
    return aTextEditor->CanDelete();
  }
  return isEnabled;
}

nsresult DeleteCommand::DoCommand(const char* aCommandName,
                                  TextEditor& aTextEditor) const {
  nsIEditor::EDirection deleteDir = nsIEditor::eNone;
  if (!strcmp("cmd_delete", aCommandName)) {
    // Really this should probably be eNone, but it only makes a difference if
    // the selection is collapsed, and then this command is disabled.  So let's
    // keep it as it always was to avoid breaking things.
    deleteDir = nsIEditor::ePrevious;
  } else if (!strcmp("cmd_deleteCharForward", aCommandName)) {
    deleteDir = nsIEditor::eNext;
  } else if (!strcmp("cmd_deleteCharBackward", aCommandName)) {
    deleteDir = nsIEditor::ePrevious;
  } else if (!strcmp("cmd_deleteWordBackward", aCommandName)) {
    deleteDir = nsIEditor::ePreviousWord;
  } else if (!strcmp("cmd_deleteWordForward", aCommandName)) {
    deleteDir = nsIEditor::eNextWord;
  } else if (!strcmp("cmd_deleteToBeginningOfLine", aCommandName)) {
    deleteDir = nsIEditor::eToBeginningOfLine;
  } else if (!strcmp("cmd_deleteToEndOfLine", aCommandName)) {
    deleteDir = nsIEditor::eToEndOfLine;
  } else {
    MOZ_CRASH("Unrecognized nsDeleteCommand");
  }
  nsresult rv =
      aTextEditor.DeleteSelectionAsAction(deleteDir, nsIEditor::eStrip);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult DeleteCommand::DoCommandParams(const char* aCommandName,
                                        nsCommandParams* aParams,
                                        TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult DeleteCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::SelectAllCommand
 ******************************************************************************/

StaticRefPtr<SelectAllCommand> SelectAllCommand::sInstance;

bool SelectAllCommand::IsCommandEnabled(const char* aCommandName,
                                        TextEditor* aTextEditor) const {
  // You can always select all, unless the selection is editable,
  // and the editable region is empty!
  if (!aTextEditor) {
    return true;
  }

  // You can select all if there is an editor which is non-empty
  bool isEmpty = false;
  if (NS_WARN_IF(NS_FAILED(aTextEditor->IsEmpty(&isEmpty)))) {
    return false;
  }
  return !isEmpty;
}

nsresult SelectAllCommand::DoCommand(const char* aCommandName,
                                     TextEditor& aTextEditor) const {
  return aTextEditor.SelectAll();
}

nsresult SelectAllCommand::DoCommandParams(const char* aCommandName,
                                           nsCommandParams* aParams,
                                           TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult SelectAllCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::SelectionMoveCommands
 ******************************************************************************/

StaticRefPtr<SelectionMoveCommands> SelectionMoveCommands::sInstance;

bool SelectionMoveCommands::IsCommandEnabled(const char* aCommandName,
                                             TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

static const struct ScrollCommand {
  const char* reverseScroll;
  const char* forwardScroll;
  nsresult (NS_STDCALL nsISelectionController::*scroll)(bool);
} scrollCommands[] = {{"cmd_scrollTop", "cmd_scrollBottom",
                       &nsISelectionController::CompleteScroll},
                      {"cmd_scrollPageUp", "cmd_scrollPageDown",
                       &nsISelectionController::ScrollPage},
                      {"cmd_scrollLineUp", "cmd_scrollLineDown",
                       &nsISelectionController::ScrollLine}};

static const struct MoveCommand {
  const char* reverseMove;
  const char* forwardMove;
  const char* reverseSelect;
  const char* forwardSelect;
  nsresult (NS_STDCALL nsISelectionController::*move)(bool, bool);
} moveCommands[] = {
    {"cmd_charPrevious", "cmd_charNext", "cmd_selectCharPrevious",
     "cmd_selectCharNext", &nsISelectionController::CharacterMove},
    {"cmd_linePrevious", "cmd_lineNext", "cmd_selectLinePrevious",
     "cmd_selectLineNext", &nsISelectionController::LineMove},
    {"cmd_wordPrevious", "cmd_wordNext", "cmd_selectWordPrevious",
     "cmd_selectWordNext", &nsISelectionController::WordMove},
    {"cmd_beginLine", "cmd_endLine", "cmd_selectBeginLine", "cmd_selectEndLine",
     &nsISelectionController::IntraLineMove},
    {"cmd_movePageUp", "cmd_movePageDown", "cmd_selectPageUp",
     "cmd_selectPageDown", &nsISelectionController::PageMove},
    {"cmd_moveTop", "cmd_moveBottom", "cmd_selectTop", "cmd_selectBottom",
     &nsISelectionController::CompleteMove}};

static const struct PhysicalCommand {
  const char* move;
  const char* select;
  int16_t direction;
  int16_t amount;
} physicalCommands[] = {
    {"cmd_moveLeft", "cmd_selectLeft", nsISelectionController::MOVE_LEFT, 0},
    {"cmd_moveRight", "cmd_selectRight", nsISelectionController::MOVE_RIGHT, 0},
    {"cmd_moveUp", "cmd_selectUp", nsISelectionController::MOVE_UP, 0},
    {"cmd_moveDown", "cmd_selectDown", nsISelectionController::MOVE_DOWN, 0},
    {"cmd_moveLeft2", "cmd_selectLeft2", nsISelectionController::MOVE_LEFT, 1},
    {"cmd_moveRight2", "cmd_selectRight2", nsISelectionController::MOVE_RIGHT,
     1},
    {"cmd_moveUp2", "cmd_selectUp2", nsISelectionController::MOVE_UP, 1},
    {"cmd_moveDown2", "cmd_selectDown2", nsISelectionController::MOVE_DOWN, 1}};

nsresult SelectionMoveCommands::DoCommand(const char* aCommandName,
                                          TextEditor& aTextEditor) const {
  RefPtr<Document> document = aTextEditor.GetDocument();
  if (document) {
    // Most of the commands below (possibly all of them) need layout to
    // be up to date.
    document->FlushPendingNotifications(FlushType::Layout);
  }

  nsCOMPtr<nsISelectionController> selectionController =
      aTextEditor.GetSelectionController();
  if (NS_WARN_IF(!selectionController)) {
    return NS_ERROR_FAILURE;
  }

  // scroll commands
  for (size_t i = 0; i < ArrayLength(scrollCommands); i++) {
    const ScrollCommand& cmd = scrollCommands[i];
    if (!strcmp(aCommandName, cmd.reverseScroll)) {
      return (selectionController->*(cmd.scroll))(false);
    } else if (!strcmp(aCommandName, cmd.forwardScroll)) {
      return (selectionController->*(cmd.scroll))(true);
    }
  }

  // caret movement/selection commands
  for (size_t i = 0; i < ArrayLength(moveCommands); i++) {
    const MoveCommand& cmd = moveCommands[i];
    if (!strcmp(aCommandName, cmd.reverseMove)) {
      return (selectionController->*(cmd.move))(false, false);
    } else if (!strcmp(aCommandName, cmd.forwardMove)) {
      return (selectionController->*(cmd.move))(true, false);
    } else if (!strcmp(aCommandName, cmd.reverseSelect)) {
      return (selectionController->*(cmd.move))(false, true);
    } else if (!strcmp(aCommandName, cmd.forwardSelect)) {
      return (selectionController->*(cmd.move))(true, true);
    }
  }

  // physical-direction movement/selection
  for (size_t i = 0; i < ArrayLength(physicalCommands); i++) {
    const PhysicalCommand& cmd = physicalCommands[i];
    if (!strcmp(aCommandName, cmd.move)) {
      return selectionController->PhysicalMove(cmd.direction, cmd.amount,
                                               false);
    } else if (!strcmp(aCommandName, cmd.select)) {
      return selectionController->PhysicalMove(cmd.direction, cmd.amount, true);
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult SelectionMoveCommands::DoCommandParams(const char* aCommandName,
                                                nsCommandParams* aParams,
                                                TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult SelectionMoveCommands::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::InsertPlaintextCommand
 ******************************************************************************/

StaticRefPtr<InsertPlaintextCommand> InsertPlaintextCommand::sInstance;

bool InsertPlaintextCommand::IsCommandEnabled(const char* aCommandName,
                                              TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult InsertPlaintextCommand::DoCommand(const char* aCommandName,
                                           TextEditor& aTextEditor) const {
  // XXX InsertTextAsAction() is not same as OnInputText().  However, other
  //     commands to insert line break or paragraph separator use OnInput*().
  //     According to the semantics of those methods, using *AsAction() is
  //     better, however, this may not cause two or more placeholder
  //     transactions to the top transaction since its name may not be
  //     nsGkAtoms::TypingTxnName.
  DebugOnly<nsresult> rv = aTextEditor.InsertTextAsAction(EmptyString());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to insert empty string");
  return NS_OK;
}

nsresult InsertPlaintextCommand::DoCommandParams(
    const char* aCommandName, nsCommandParams* aParams,
    TextEditor& aTextEditor) const {
  if (NS_WARN_IF(!aParams)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Get text to insert from command params
  nsAutoString text;
  nsresult rv = aParams->GetString(STATE_DATA, text);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // XXX InsertTextAsAction() is not same as OnInputText().  However, other
  //     commands to insert line break or paragraph separator use OnInput*().
  //     According to the semantics of those methods, using *AsAction() is
  //     better, however, this may not cause two or more placeholder
  //     transactions to the top transaction since its name may not be
  //     nsGkAtoms::TypingTxnName.
  rv = aTextEditor.InsertTextAsAction(text);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to insert the text");
  return NS_OK;
}

nsresult InsertPlaintextCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::InsertParagraphCommand
 ******************************************************************************/

StaticRefPtr<InsertParagraphCommand> InsertParagraphCommand::sInstance;

bool InsertParagraphCommand::IsCommandEnabled(const char* aCommandName,
                                              TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult InsertParagraphCommand::DoCommand(const char* aCommandName,
                                           TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;  // Do nothing for now.
  }
  return htmlEditor->InsertParagraphSeparatorAsAction();
}

nsresult InsertParagraphCommand::DoCommandParams(
    const char* aCommandName, nsCommandParams* aParams,
    TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult InsertParagraphCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::InsertLineBreakCommand
 ******************************************************************************/

StaticRefPtr<InsertLineBreakCommand> InsertLineBreakCommand::sInstance;

bool InsertLineBreakCommand::IsCommandEnabled(const char* aCommandName,
                                              TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult InsertLineBreakCommand::DoCommand(const char* aCommandName,
                                           TextEditor& aTextEditor) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (!htmlEditor) {
    return NS_ERROR_FAILURE;
  }
  return htmlEditor->InsertLineBreakAsAction();
}

nsresult InsertLineBreakCommand::DoCommandParams(
    const char* aCommandName, nsCommandParams* aParams,
    TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult InsertLineBreakCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommandName, aTextEditor));
}

/******************************************************************************
 * mozilla::PasteQuotationCommand
 ******************************************************************************/

StaticRefPtr<PasteQuotationCommand> PasteQuotationCommand::sInstance;

bool PasteQuotationCommand::IsCommandEnabled(const char* aCommandName,
                                             TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return !aTextEditor->IsSingleLineEditor() &&
         aTextEditor->CanPaste(nsIClipboard::kGlobalClipboard);
}

nsresult PasteQuotationCommand::DoCommand(const char* aCommandName,
                                          TextEditor& aTextEditor) const {
  nsresult rv = aTextEditor.PasteAsQuotationAsAction(
      nsIClipboard::kGlobalClipboard, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult PasteQuotationCommand::DoCommandParams(const char* aCommandName,
                                                nsCommandParams* aParams,
                                                TextEditor& aTextEditor) const {
  return DoCommand(aCommandName, aTextEditor);
}

nsresult PasteQuotationCommand::GetCommandStateParams(
    const char* aCommandName, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  if (!aTextEditor) {
    return NS_OK;
  }
  aParams.SetBool(STATE_ENABLED,
                  aTextEditor->CanPaste(nsIClipboard::kGlobalClipboard));
  return NS_OK;
}

}  // namespace mozilla
