/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorCommands.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/FlushType.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"  // for mozilla::detail::Any
#include "mozilla/TextEditor.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Selection.h"
#include "nsCommandParams.h"
#include "nsIClipboard.h"
#include "nsIEditingSession.h"
#include "nsIPrincipal.h"
#include "nsISelectionController.h"
#include "nsITransferable.h"
#include "nsString.h"
#include "nsAString.h"

class nsISupports;

#define STATE_ENABLED "state_enabled"
#define STATE_ATTRIBUTE "state_attribute"
#define STATE_DATA "state_data"

namespace mozilla {

using detail::Any;

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
  *aIsEnabled = IsCommandEnabled(GetInternalCommand(aCommandName),
                                 MOZ_KnownLive(textEditor));
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
  nsresult rv = DoCommand(GetInternalCommand(aCommandName),
                          MOZ_KnownLive(*editor->AsTextEditor()), nullptr);
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
  nsCommandParams* params = aParams ? aParams->AsCommandParams() : nullptr;
  Command command = GetInternalCommand(aCommandName, params);
  EditorCommandParamType paramType = EditorCommand::GetParamType(command);
  if (paramType == EditorCommandParamType::None) {
    nsresult rv = DoCommandParam(
        command, MOZ_KnownLive(*editor->AsTextEditor()), nullptr);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to do command from nsIControllerCommand::DoCommandParams()");
    return rv;
  }

  if (Any(paramType & EditorCommandParamType::Bool)) {
    if (Any(paramType & EditorCommandParamType::StateAttribute)) {
      Maybe<bool> boolParam = Nothing();
      if (params) {
        ErrorResult error;
        boolParam = Some(params->GetBool(STATE_ATTRIBUTE, error));
        if (NS_WARN_IF(error.Failed())) {
          return error.StealNSResult();
        }
      }
      nsresult rv = DoCommandParam(
          command, boolParam, MOZ_KnownLive(*editor->AsTextEditor()), nullptr);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "Failed to do command from nsIControllerCommand::DoCommandParams()");
      return rv;
    }
    MOZ_ASSERT_UNREACHABLE("Unexpected state for bool");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Special case for MultiStateCommandBase.  It allows both CString and String
  // in STATE_ATTRIBUTE and CString is preferred.
  if (Any(paramType & EditorCommandParamType::CString) &&
      Any(paramType & EditorCommandParamType::String)) {
    if (!params) {
      nsresult rv =
          DoCommandParam(command, VoidString(),
                         MOZ_KnownLive(*editor->AsTextEditor()), nullptr);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "Failed to do command from "
                           "nsIControllerCommand::DoCommandParams()");
      return rv;
    }
    if (Any(paramType & EditorCommandParamType::StateAttribute)) {
      nsCString cStringParam;
      nsresult rv = params->GetCString(STATE_ATTRIBUTE, cStringParam);
      if (NS_SUCCEEDED(rv)) {
        NS_ConvertUTF8toUTF16 stringParam(cStringParam);
        nsresult rv =
            DoCommandParam(command, stringParam,
                           MOZ_KnownLive(*editor->AsTextEditor()), nullptr);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "Failed to do command from "
                             "nsIControllerCommand::DoCommandParams()");
        return rv;
      }
      nsString stringParam;
      DebugOnly<nsresult> rvIgnored =
          params->GetString(STATE_ATTRIBUTE, stringParam);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "Failed to get string from STATE_ATTRIBUTE");
      rv = DoCommandParam(command, stringParam,
                          MOZ_KnownLive(*editor->AsTextEditor()), nullptr);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "Failed to do command from "
                           "nsIControllerCommand::DoCommandParams()");
      return rv;
    }
    MOZ_ASSERT_UNREACHABLE("Unexpected state for CString/String");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (Any(paramType & EditorCommandParamType::CString)) {
    if (!params) {
      nsresult rv =
          DoCommandParam(command, VoidCString(),
                         MOZ_KnownLive(*editor->AsTextEditor()), nullptr);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "Failed to do command from nsIControllerCommand::DoCommandParams()");
      return rv;
    }
    if (Any(paramType & EditorCommandParamType::StateAttribute)) {
      nsCString cStringParam;
      nsresult rv = params->GetCString(STATE_ATTRIBUTE, cStringParam);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      rv = DoCommandParam(command, cStringParam,
                          MOZ_KnownLive(*editor->AsTextEditor()), nullptr);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "Failed to do command from nsIControllerCommand::DoCommandParams()");
      return rv;
    }
    MOZ_ASSERT_UNREACHABLE("Unexpected state for CString");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (Any(paramType & EditorCommandParamType::String)) {
    if (!params) {
      nsresult rv =
          DoCommandParam(command, VoidString(),
                         MOZ_KnownLive(*editor->AsTextEditor()), nullptr);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "Failed to do command from nsIControllerCommand::DoCommandParams()");
      return rv;
    }
    nsString stringParam;
    if (Any(paramType & EditorCommandParamType::StateAttribute)) {
      nsresult rv = params->GetString(STATE_ATTRIBUTE, stringParam);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (Any(paramType & EditorCommandParamType::StateData)) {
      nsresult rv = params->GetString(STATE_DATA, stringParam);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      MOZ_ASSERT_UNREACHABLE("Unexpected state for String");
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    nsresult rv = DoCommandParam(
        command, stringParam, MOZ_KnownLive(*editor->AsTextEditor()), nullptr);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to do command from nsIControllerCommand::DoCommandParams()");
    return rv;
  }

  if (Any(paramType & EditorCommandParamType::Transferable)) {
    nsCOMPtr<nsITransferable> transferable;
    if (params) {
      nsCOMPtr<nsISupports> supports = params->GetISupports("transferable");
      transferable = do_QueryInterface(supports);
    }
    nsresult rv = DoCommandParam(
        command, transferable, MOZ_KnownLive(*editor->AsTextEditor()), nullptr);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to do command from nsIControllerCommand::DoCommandParams()");
    return rv;
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected param type");
  return NS_ERROR_NOT_IMPLEMENTED;
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
    return GetCommandStateParams(GetInternalCommand(aCommandName),
                                 MOZ_KnownLive(*aParams->AsCommandParams()),
                                 MOZ_KnownLive(editor->AsTextEditor()),
                                 nullptr);
  }
  nsCOMPtr<nsIEditingSession> editingSession =
      do_QueryInterface(aCommandRefCon);
  if (editingSession) {
    return GetCommandStateParams(GetInternalCommand(aCommandName),
                                 MOZ_KnownLive(*aParams->AsCommandParams()),
                                 nullptr, editingSession);
  }
  return GetCommandStateParams(GetInternalCommand(aCommandName),
                               MOZ_KnownLive(*aParams->AsCommandParams()),
                               nullptr, nullptr);
}

/******************************************************************************
 * mozilla::UndoCommand
 ******************************************************************************/

StaticRefPtr<UndoCommand> UndoCommand::sInstance;

bool UndoCommand::IsCommandEnabled(Command aCommand,
                                   TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable() && aTextEditor->CanUndo();
}

nsresult UndoCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                                nsIPrincipal* aPrincipal) const {
  nsresult rv = aTextEditor.UndoAsAction(1, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "UndoAsAction() failed");
  return rv;
}

nsresult UndoCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::RedoCommand
 ******************************************************************************/

StaticRefPtr<RedoCommand> RedoCommand::sInstance;

bool RedoCommand::IsCommandEnabled(Command aCommand,
                                   TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable() && aTextEditor->CanRedo();
}

nsresult RedoCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                                nsIPrincipal* aPrincipal) const {
  nsresult rv = aTextEditor.RedoAsAction(1, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "RedoAsAction() failed");
  return rv;
}

nsresult RedoCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::CutCommand
 ******************************************************************************/

StaticRefPtr<CutCommand> CutCommand::sInstance;

bool CutCommand::IsCommandEnabled(Command aCommand,
                                  TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable() &&
         aTextEditor->IsCutCommandEnabled();
}

nsresult CutCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                               nsIPrincipal* aPrincipal) const {
  nsresult rv = aTextEditor.CutAsAction(aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "CutAsAction() failed");
  return rv;
}

nsresult CutCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::CutOrDeleteCommand
 ******************************************************************************/

StaticRefPtr<CutOrDeleteCommand> CutOrDeleteCommand::sInstance;

bool CutOrDeleteCommand::IsCommandEnabled(Command aCommand,
                                          TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult CutOrDeleteCommand::DoCommand(Command aCommand,
                                       TextEditor& aTextEditor,
                                       nsIPrincipal* aPrincipal) const {
  dom::Selection* selection = aTextEditor.GetSelection();
  if (selection && selection->IsCollapsed()) {
    nsresult rv = aTextEditor.DeleteSelectionAsAction(
        nsIEditor::eNext, nsIEditor::eStrip, aPrincipal);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "DeleteSelectionsAction() failed");
    return rv;
  }
  nsresult rv = aTextEditor.CutAsAction(aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "CutAsAction() failed");
  return rv;
}

nsresult CutOrDeleteCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::CopyCommand
 ******************************************************************************/

StaticRefPtr<CopyCommand> CopyCommand::sInstance;

bool CopyCommand::IsCommandEnabled(Command aCommand,
                                   TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsCopyCommandEnabled();
}

nsresult CopyCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                                nsIPrincipal* aPrincipal) const {
  // Shouldn't cause "beforeinput" event so that we don't need to specify
  // the given principal.
  return aTextEditor.Copy();
}

nsresult CopyCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::CopyOrDeleteCommand
 ******************************************************************************/

StaticRefPtr<CopyOrDeleteCommand> CopyOrDeleteCommand::sInstance;

bool CopyOrDeleteCommand::IsCommandEnabled(Command aCommand,
                                           TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult CopyOrDeleteCommand::DoCommand(Command aCommand,
                                        TextEditor& aTextEditor,
                                        nsIPrincipal* aPrincipal) const {
  dom::Selection* selection = aTextEditor.GetSelection();
  if (selection && selection->IsCollapsed()) {
    nsresult rv = aTextEditor.DeleteSelectionAsAction(
        nsIEditor::eNextWord, nsIEditor::eStrip, aPrincipal);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "DeleteSelectionAsAction() failed");
    return rv;
  }
  // Shouldn't cause "beforeinput" event so that we don't need to specify
  // the given principal.
  nsresult rv = aTextEditor.Copy();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Copy() failed");
  return rv;
}

nsresult CopyOrDeleteCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::PasteCommand
 ******************************************************************************/

StaticRefPtr<PasteCommand> PasteCommand::sInstance;

bool PasteCommand::IsCommandEnabled(Command aCommand,
                                    TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable() &&
         aTextEditor->CanPaste(nsIClipboard::kGlobalClipboard);
}

nsresult PasteCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                                 nsIPrincipal* aPrincipal) const {
  nsresult rv = aTextEditor.PasteAsAction(nsIClipboard::kGlobalClipboard, true,
                                          aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "PasteAsAction() failed");
  return rv;
}

nsresult PasteCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::PasteTransferableCommand
 ******************************************************************************/

StaticRefPtr<PasteTransferableCommand> PasteTransferableCommand::sInstance;

bool PasteTransferableCommand::IsCommandEnabled(Command aCommand,
                                                TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable() &&
         aTextEditor->CanPasteTransferable(nullptr);
}

nsresult PasteTransferableCommand::DoCommand(Command aCommand,
                                             TextEditor& aTextEditor,
                                             nsIPrincipal* aPrincipal) const {
  return NS_ERROR_FAILURE;
}

nsresult PasteTransferableCommand::DoCommandParam(
    Command aCommand, nsITransferable* aTransferableParam,
    TextEditor& aTextEditor, nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(!aTransferableParam)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv =
      aTextEditor.PasteTransferableAsAction(aTransferableParam, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "PasteTransferableAsAction() failed");
  return rv;
}

nsresult PasteTransferableCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
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
    Command aCommand, TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult SwitchTextDirectionCommand::DoCommand(Command aCommand,
                                               TextEditor& aTextEditor,
                                               nsIPrincipal* aPrincipal) const {
  nsresult rv = aTextEditor.ToggleTextDirectionAsAction(aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "ToggleTextDirectionAsAction() failed");
  return rv;
}

nsresult SwitchTextDirectionCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::DeleteCommand
 ******************************************************************************/

StaticRefPtr<DeleteCommand> DeleteCommand::sInstance;

bool DeleteCommand::IsCommandEnabled(Command aCommand,
                                     TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  // We can generally delete whenever the selection is editable.  However,
  // cmd_delete doesn't make sense if the selection is collapsed because it's
  // directionless.
  bool isEnabled = aTextEditor->IsSelectionEditable();

  if (aCommand == Command::Delete && isEnabled) {
    return aTextEditor->CanDeleteSelection();
  }
  return isEnabled;
}

nsresult DeleteCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                                  nsIPrincipal* aPrincipal) const {
  nsIEditor::EDirection deleteDir = nsIEditor::eNone;
  switch (aCommand) {
    case Command::Delete:
      // Really this should probably be eNone, but it only makes a difference
      // if the selection is collapsed, and then this command is disabled.  So
      // let's keep it as it always was to avoid breaking things.
      deleteDir = nsIEditor::ePrevious;
      break;
    case Command::DeleteCharForward:
      deleteDir = nsIEditor::eNext;
      break;
    case Command::DeleteCharBackward:
      deleteDir = nsIEditor::ePrevious;
      break;
    case Command::DeleteWordBackward:
      deleteDir = nsIEditor::ePreviousWord;
      break;
    case Command::DeleteWordForward:
      deleteDir = nsIEditor::eNextWord;
      break;
    case Command::DeleteToBeginningOfLine:
      deleteDir = nsIEditor::eToBeginningOfLine;
      break;
    case Command::DeleteToEndOfLine:
      deleteDir = nsIEditor::eToEndOfLine;
      break;
    default:
      MOZ_CRASH("Unrecognized nsDeleteCommand");
  }
  nsresult rv = aTextEditor.DeleteSelectionAsAction(
      deleteDir, nsIEditor::eStrip, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "DeleteSelectionAsAction() failed");
  return rv;
}

nsresult DeleteCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::SelectAllCommand
 ******************************************************************************/

StaticRefPtr<SelectAllCommand> SelectAllCommand::sInstance;

bool SelectAllCommand::IsCommandEnabled(Command aCommand,
                                        TextEditor* aTextEditor) const {
  // You can always select all, unless the selection is editable,
  // and the editable region is empty!
  if (!aTextEditor) {
    return true;
  }

  // You can select all if there is an editor which is non-empty
  return !aTextEditor->IsEmpty();
}

nsresult SelectAllCommand::DoCommand(Command aCommand, TextEditor& aTextEditor,
                                     nsIPrincipal* aPrincipal) const {
  // Shouldn't cause "beforeinput" event so that we don't need to specify
  // aPrincipal.
  return aTextEditor.SelectAll();
}

nsresult SelectAllCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::SelectionMoveCommands
 ******************************************************************************/

StaticRefPtr<SelectionMoveCommands> SelectionMoveCommands::sInstance;

bool SelectionMoveCommands::IsCommandEnabled(Command aCommand,
                                             TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

static const struct ScrollCommand {
  Command mReverseScroll;
  Command mForwardScroll;
  nsresult (NS_STDCALL nsISelectionController::*scroll)(bool);
} scrollCommands[] = {{Command::ScrollTop, Command::ScrollBottom,
                       &nsISelectionController::CompleteScroll},
                      {Command::ScrollPageUp, Command::ScrollPageDown,
                       &nsISelectionController::ScrollPage},
                      {Command::ScrollLineUp, Command::ScrollLineDown,
                       &nsISelectionController::ScrollLine}};

static const struct MoveCommand {
  Command mReverseMove;
  Command mForwardMove;
  Command mReverseSelect;
  Command mForwardSelect;
  nsresult (NS_STDCALL nsISelectionController::*move)(bool, bool);
} moveCommands[] = {
    {Command::CharPrevious, Command::CharNext, Command::SelectCharPrevious,
     Command::SelectCharNext, &nsISelectionController::CharacterMove},
    {Command::LinePrevious, Command::LineNext, Command::SelectLinePrevious,
     Command::SelectLineNext, &nsISelectionController::LineMove},
    {Command::WordPrevious, Command::WordNext, Command::SelectWordPrevious,
     Command::SelectWordNext, &nsISelectionController::WordMove},
    {Command::BeginLine, Command::EndLine, Command::SelectBeginLine,
     Command::SelectEndLine, &nsISelectionController::IntraLineMove},
    {Command::MovePageUp, Command::MovePageDown, Command::SelectPageUp,
     Command::SelectPageDown, &nsISelectionController::PageMove},
    {Command::MoveTop, Command::MoveBottom, Command::SelectTop,
     Command::SelectBottom, &nsISelectionController::CompleteMove}};

static const struct PhysicalCommand {
  Command mMove;
  Command mSelect;
  int16_t direction;
  int16_t amount;
} physicalCommands[] = {
    {Command::MoveLeft, Command::SelectLeft, nsISelectionController::MOVE_LEFT,
     0},
    {Command::MoveRight, Command::SelectRight,
     nsISelectionController::MOVE_RIGHT, 0},
    {Command::MoveUp, Command::SelectUp, nsISelectionController::MOVE_UP, 0},
    {Command::MoveDown, Command::SelectDown, nsISelectionController::MOVE_DOWN,
     0},
    {Command::MoveLeft2, Command::SelectLeft2,
     nsISelectionController::MOVE_LEFT, 1},
    {Command::MoveRight2, Command::SelectRight2,
     nsISelectionController::MOVE_RIGHT, 1},
    {Command::MoveUp2, Command::SelectUp2, nsISelectionController::MOVE_UP, 1},
    {Command::MoveDown2, Command::SelectDown2,
     nsISelectionController::MOVE_DOWN, 1}};

nsresult SelectionMoveCommands::DoCommand(Command aCommand,
                                          TextEditor& aTextEditor,
                                          nsIPrincipal* aPrincipal) const {
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
    if (aCommand == cmd.mReverseScroll) {
      return (selectionController->*(cmd.scroll))(false);
    }
    if (aCommand == cmd.mForwardScroll) {
      return (selectionController->*(cmd.scroll))(true);
    }
  }

  // caret movement/selection commands
  for (size_t i = 0; i < ArrayLength(moveCommands); i++) {
    const MoveCommand& cmd = moveCommands[i];
    if (aCommand == cmd.mReverseMove) {
      return (selectionController->*(cmd.move))(false, false);
    }
    if (aCommand == cmd.mForwardMove) {
      return (selectionController->*(cmd.move))(true, false);
    }
    if (aCommand == cmd.mReverseSelect) {
      return (selectionController->*(cmd.move))(false, true);
    }
    if (aCommand == cmd.mForwardSelect) {
      return (selectionController->*(cmd.move))(true, true);
    }
  }

  // physical-direction movement/selection
  for (size_t i = 0; i < ArrayLength(physicalCommands); i++) {
    const PhysicalCommand& cmd = physicalCommands[i];
    if (aCommand == cmd.mMove) {
      return selectionController->PhysicalMove(cmd.direction, cmd.amount,
                                               false);
    }
    if (aCommand == cmd.mSelect) {
      return selectionController->PhysicalMove(cmd.direction, cmd.amount, true);
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult SelectionMoveCommands::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::InsertPlaintextCommand
 ******************************************************************************/

StaticRefPtr<InsertPlaintextCommand> InsertPlaintextCommand::sInstance;

bool InsertPlaintextCommand::IsCommandEnabled(Command aCommand,
                                              TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult InsertPlaintextCommand::DoCommand(Command aCommand,
                                           TextEditor& aTextEditor,
                                           nsIPrincipal* aPrincipal) const {
  // XXX InsertTextAsAction() is not same as OnInputText().  However, other
  //     commands to insert line break or paragraph separator use OnInput*().
  //     According to the semantics of those methods, using *AsAction() is
  //     better, however, this may not cause two or more placeholder
  //     transactions to the top transaction since its name may not be
  //     nsGkAtoms::TypingTxnName.
  DebugOnly<nsresult> rv =
      aTextEditor.InsertTextAsAction(EmptyString(), aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "InsertTextAsAction() failed");
  return NS_OK;
}

nsresult InsertPlaintextCommand::DoCommandParam(
    Command aCommand, const nsAString& aStringParam, TextEditor& aTextEditor,
    nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(aStringParam.IsVoid())) {
    return NS_ERROR_INVALID_ARG;
  }

  // XXX InsertTextAsAction() is not same as OnInputText().  However, other
  //     commands to insert line break or paragraph separator use OnInput*().
  //     According to the semantics of those methods, using *AsAction() is
  //     better, however, this may not cause two or more placeholder
  //     transactions to the top transaction since its name may not be
  //     nsGkAtoms::TypingTxnName.
  DebugOnly<nsresult> rv =
      aTextEditor.InsertTextAsAction(aStringParam, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "InsertTextAsAction() failed");
  return NS_OK;
}

nsresult InsertPlaintextCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::InsertParagraphCommand
 ******************************************************************************/

StaticRefPtr<InsertParagraphCommand> InsertParagraphCommand::sInstance;

bool InsertParagraphCommand::IsCommandEnabled(Command aCommand,
                                              TextEditor* aTextEditor) const {
  if (!aTextEditor || !aTextEditor->AsHTMLEditor()) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult InsertParagraphCommand::DoCommand(Command aCommand,
                                           TextEditor& aTextEditor,
                                           nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (!htmlEditor) {
    return NS_OK;  // Do nothing for now.
  }
  nsresult rv =
      MOZ_KnownLive(htmlEditor)->InsertParagraphSeparatorAsAction(aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "InsertParagraphSeparatorAsAction() failed");
  return rv;
}

nsresult InsertParagraphCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::InsertLineBreakCommand
 ******************************************************************************/

StaticRefPtr<InsertLineBreakCommand> InsertLineBreakCommand::sInstance;

bool InsertLineBreakCommand::IsCommandEnabled(Command aCommand,
                                              TextEditor* aTextEditor) const {
  if (!aTextEditor || !aTextEditor->AsHTMLEditor()) {
    return false;
  }
  return aTextEditor->IsSelectionEditable();
}

nsresult InsertLineBreakCommand::DoCommand(Command aCommand,
                                           TextEditor& aTextEditor,
                                           nsIPrincipal* aPrincipal) const {
  HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
  if (!htmlEditor) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = MOZ_KnownLive(htmlEditor)->InsertLineBreakAsAction(aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "InsertLineBreakAsAction() failed");
  return rv;
}

nsresult InsertLineBreakCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  return aParams.SetBool(STATE_ENABLED,
                         IsCommandEnabled(aCommand, aTextEditor));
}

/******************************************************************************
 * mozilla::PasteQuotationCommand
 ******************************************************************************/

StaticRefPtr<PasteQuotationCommand> PasteQuotationCommand::sInstance;

bool PasteQuotationCommand::IsCommandEnabled(Command aCommand,
                                             TextEditor* aTextEditor) const {
  if (!aTextEditor) {
    return false;
  }
  return !aTextEditor->IsSingleLineEditor() &&
         aTextEditor->CanPaste(nsIClipboard::kGlobalClipboard);
}

nsresult PasteQuotationCommand::DoCommand(Command aCommand,
                                          TextEditor& aTextEditor,
                                          nsIPrincipal* aPrincipal) const {
  nsresult rv = aTextEditor.PasteAsQuotationAsAction(
      nsIClipboard::kGlobalClipboard, true, aPrincipal);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "PasteAsQuotationAsAction() failed");
  return rv;
}

nsresult PasteQuotationCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  if (!aTextEditor) {
    return NS_OK;
  }
  aParams.SetBool(STATE_ENABLED,
                  aTextEditor->CanPaste(nsIClipboard::kGlobalClipboard));
  return NS_OK;
}

}  // namespace mozilla
