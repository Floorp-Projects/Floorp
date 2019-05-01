/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditorController.h"

#include "mozilla/EditorCommands.h"      // for StyleUpdatingCommand, etc
#include "mozilla/mozalloc.h"            // for operator new
#include "nsControllerCommandTable.h"    // for nsControllerCommandTable
#include "nsError.h"                     // for NS_OK

namespace mozilla {

#define NS_REGISTER_COMMAND(_cmdClass, _cmdName)                       \
  {                                                                    \
    aCommandTable->RegisterCommand(                                    \
        _cmdName,                                                      \
        static_cast<nsIControllerCommand*>(_cmdClass::GetInstance())); \
  }

// static
nsresult HTMLEditorController::RegisterEditorDocStateCommands(
    nsControllerCommandTable* aCommandTable) {
  // observer commands for document state
  NS_REGISTER_COMMAND(DocumentStateCommand, "obs_documentCreated")
  NS_REGISTER_COMMAND(DocumentStateCommand, "obs_documentWillBeDestroyed")
  NS_REGISTER_COMMAND(DocumentStateCommand, "obs_documentLocationChanged")

  // commands that may get or change state
  NS_REGISTER_COMMAND(SetDocumentStateCommand, "cmd_setDocumentModified")
  NS_REGISTER_COMMAND(SetDocumentStateCommand, "cmd_setDocumentUseCSS")
  NS_REGISTER_COMMAND(SetDocumentStateCommand, "cmd_setDocumentReadOnly")
  NS_REGISTER_COMMAND(SetDocumentStateCommand, "cmd_insertBrOnReturn")
  NS_REGISTER_COMMAND(SetDocumentStateCommand, "cmd_defaultParagraphSeparator")
  NS_REGISTER_COMMAND(SetDocumentStateCommand, "cmd_enableObjectResizing")
  NS_REGISTER_COMMAND(SetDocumentStateCommand, "cmd_enableInlineTableEditing")
  NS_REGISTER_COMMAND(SetDocumentStateCommand,
                      "cmd_enableAbsolutePositionEditing")

  return NS_OK;
}

// static
nsresult HTMLEditorController::RegisterHTMLEditorCommands(
    nsControllerCommandTable* aCommandTable) {
  // Edit menu
  NS_REGISTER_COMMAND(PasteNoFormattingCommand, "cmd_pasteNoFormatting");

  // indent/outdent
  NS_REGISTER_COMMAND(IndentCommand, "cmd_indent");
  NS_REGISTER_COMMAND(OutdentCommand, "cmd_outdent");

  // Styles
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_bold");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_italic");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_underline");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_tt");

  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_strikethrough");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_superscript");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_subscript");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_nobreak");

  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_em");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_strong");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_cite");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_abbr");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_acronym");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_code");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_samp");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_var");
  NS_REGISTER_COMMAND(StyleUpdatingCommand, "cmd_removeLinks");

  // lists
  NS_REGISTER_COMMAND(ListCommand, "cmd_ol");
  NS_REGISTER_COMMAND(ListCommand, "cmd_ul");
  NS_REGISTER_COMMAND(ListItemCommand, "cmd_dt");
  NS_REGISTER_COMMAND(ListItemCommand, "cmd_dd");
  NS_REGISTER_COMMAND(RemoveListCommand, "cmd_removeList");

  // format stuff
  NS_REGISTER_COMMAND(ParagraphStateCommand, "cmd_paragraphState");
  NS_REGISTER_COMMAND(FontFaceStateCommand, "cmd_fontFace");
  NS_REGISTER_COMMAND(FontSizeStateCommand, "cmd_fontSize");
  NS_REGISTER_COMMAND(FontColorStateCommand, "cmd_fontColor");
  NS_REGISTER_COMMAND(BackgroundColorStateCommand, "cmd_backgroundColor");
  NS_REGISTER_COMMAND(HighlightColorStateCommand, "cmd_highlight");

  NS_REGISTER_COMMAND(AlignCommand, "cmd_align");
  NS_REGISTER_COMMAND(RemoveStylesCommand, "cmd_removeStyles");

  NS_REGISTER_COMMAND(IncreaseFontSizeCommand, "cmd_increaseFont");
  NS_REGISTER_COMMAND(DecreaseFontSizeCommand, "cmd_decreaseFont");

  // Insert content
  NS_REGISTER_COMMAND(InsertHTMLCommand, "cmd_insertHTML");
  NS_REGISTER_COMMAND(InsertTagCommand, "cmd_insertLinkNoUI");
  NS_REGISTER_COMMAND(InsertTagCommand, "cmd_insertImageNoUI");
  NS_REGISTER_COMMAND(InsertTagCommand, "cmd_insertHR");

  NS_REGISTER_COMMAND(AbsolutePositioningCommand, "cmd_absPos");
  NS_REGISTER_COMMAND(DecreaseZIndexCommand, "cmd_decreaseZIndex");
  NS_REGISTER_COMMAND(IncreaseZIndexCommand, "cmd_increaseZIndex");

  return NS_OK;
}

// static
void HTMLEditorController::Shutdown() {
  // EditorDocStateCommands
  DocumentStateCommand::Shutdown();
  SetDocumentStateCommand::Shutdown();

  // HTMLEditorCommands
  PasteNoFormattingCommand::Shutdown();
  IndentCommand::Shutdown();
  OutdentCommand::Shutdown();
  StyleUpdatingCommand::Shutdown();
  ListCommand::Shutdown();
  ListItemCommand::Shutdown();
  RemoveListCommand::Shutdown();
  ParagraphStateCommand::Shutdown();
  FontFaceStateCommand::Shutdown();
  FontSizeStateCommand::Shutdown();
  FontColorStateCommand::Shutdown();
  BackgroundColorStateCommand::Shutdown();
  HighlightColorStateCommand::Shutdown();
  AlignCommand::Shutdown();
  RemoveStylesCommand::Shutdown();
  IncreaseFontSizeCommand::Shutdown();
  DecreaseFontSizeCommand::Shutdown();
  InsertHTMLCommand::Shutdown();
  InsertTagCommand::Shutdown();
  AbsolutePositioningCommand::Shutdown();
  DecreaseZIndexCommand::Shutdown();
  IncreaseZIndexCommand::Shutdown();
}

}  // namespace mozilla
