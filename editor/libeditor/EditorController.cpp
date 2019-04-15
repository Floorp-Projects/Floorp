/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditorController.h"

#include "EditorCommands.h"
#include "mozilla/mozalloc.h"
#include "nsControllerCommandTable.h"
#include "nsDebug.h"
#include "nsError.h"

class nsIControllerCommand;

namespace mozilla {

#define NS_REGISTER_COMMAND(_cmdClass, _cmdName)                       \
  {                                                                    \
    aCommandTable->RegisterCommand(                                    \
        _cmdName,                                                      \
        static_cast<nsIControllerCommand*>(_cmdClass::GetInstance())); \
  }

// static
nsresult EditorController::RegisterEditingCommands(
    nsControllerCommandTable* aCommandTable) {
  // now register all our commands
  // These are commands that will be used in text widgets, and in composer

  NS_REGISTER_COMMAND(UndoCommand, "cmd_undo");
  NS_REGISTER_COMMAND(RedoCommand, "cmd_redo");

  NS_REGISTER_COMMAND(CutCommand, "cmd_cut");
  NS_REGISTER_COMMAND(CutOrDeleteCommand, "cmd_cutOrDelete");
  NS_REGISTER_COMMAND(CopyCommand, "cmd_copy");
  NS_REGISTER_COMMAND(CopyOrDeleteCommand, "cmd_copyOrDelete");
  NS_REGISTER_COMMAND(SelectAllCommand, "cmd_selectAll");

  NS_REGISTER_COMMAND(PasteCommand, "cmd_paste");
  NS_REGISTER_COMMAND(PasteTransferableCommand, "cmd_pasteTransferable");

  NS_REGISTER_COMMAND(SwitchTextDirectionCommand, "cmd_switchTextDirection");

  NS_REGISTER_COMMAND(DeleteCommand, "cmd_delete");
  NS_REGISTER_COMMAND(DeleteCommand, "cmd_deleteCharBackward");
  NS_REGISTER_COMMAND(DeleteCommand, "cmd_deleteCharForward");
  NS_REGISTER_COMMAND(DeleteCommand, "cmd_deleteWordBackward");
  NS_REGISTER_COMMAND(DeleteCommand, "cmd_deleteWordForward");
  NS_REGISTER_COMMAND(DeleteCommand, "cmd_deleteToBeginningOfLine");
  NS_REGISTER_COMMAND(DeleteCommand, "cmd_deleteToEndOfLine");

  // Insert content
  NS_REGISTER_COMMAND(InsertPlaintextCommand, "cmd_insertText");
  NS_REGISTER_COMMAND(InsertParagraphCommand, "cmd_insertParagraph");
  NS_REGISTER_COMMAND(InsertLineBreakCommand, "cmd_insertLineBreak");
  NS_REGISTER_COMMAND(PasteQuotationCommand, "cmd_pasteQuote");

  return NS_OK;
}

// static
nsresult EditorController::RegisterEditorCommands(
    nsControllerCommandTable* aCommandTable) {
  // These are commands that will be used in text widgets only.

  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_scrollTop");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_scrollBottom");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_moveTop");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_moveBottom");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectTop");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectBottom");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_lineNext");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_linePrevious");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectLineNext");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectLinePrevious");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_charPrevious");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_charNext");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectCharPrevious");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectCharNext");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_beginLine");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_endLine");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectBeginLine");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectEndLine");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_wordPrevious");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_wordNext");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectWordPrevious");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectWordNext");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_scrollPageUp");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_scrollPageDown");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_scrollLineUp");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_scrollLineDown");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_movePageUp");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_movePageDown");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectPageUp");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectPageDown");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_moveLeft");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_moveRight");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_moveUp");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_moveDown");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_moveLeft2");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_moveRight2");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_moveUp2");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_moveDown2");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectLeft");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectRight");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectUp");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectDown");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectLeft2");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectRight2");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectUp2");
  NS_REGISTER_COMMAND(SelectionMoveCommands, "cmd_selectDown2");

  return NS_OK;
}

// static
void EditorController::Shutdown() {
  // EditingCommands
  UndoCommand::Shutdown();
  RedoCommand::Shutdown();
  CutCommand::Shutdown();
  CutOrDeleteCommand::Shutdown();
  CopyCommand::Shutdown();
  CopyOrDeleteCommand::Shutdown();
  PasteCommand::Shutdown();
  PasteTransferableCommand::Shutdown();
  SwitchTextDirectionCommand::Shutdown();
  DeleteCommand::Shutdown();
  SelectAllCommand::Shutdown();
  InsertPlaintextCommand::Shutdown();
  InsertParagraphCommand::Shutdown();
  InsertLineBreakCommand::Shutdown();
  PasteQuotationCommand::Shutdown();

  // EditorCommands
  SelectionMoveCommands::Shutdown();
}

}  // namespace mozilla
