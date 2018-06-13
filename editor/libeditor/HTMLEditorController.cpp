/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditorController.h"

#include "mozilla/HTMLEditorCommands.h" // for StyleUpdatingCommand, etc
#include "mozilla/mozalloc.h"           // for operator new
#include "nsError.h"                    // for NS_OK
#include "nsGkAtoms.h"                  // for nsGkAtoms, nsGkAtoms::a, etc
#include "nsIControllerCommandTable.h"  // for nsIControllerCommandTable

class nsIControllerCommand;

namespace mozilla {

#define NS_REGISTER_ONE_COMMAND(_cmdClass, _cmdName)                    \
  {                                                                     \
    _cmdClass* theCmd = new _cmdClass();                                \
    inCommandTable->RegisterCommand(_cmdName,                           \
                       static_cast<nsIControllerCommand *>(theCmd));    \
  }

#define NS_REGISTER_FIRST_COMMAND(_cmdClass, _cmdName)                  \
  {                                                                     \
    _cmdClass* theCmd = new _cmdClass();                                \
    inCommandTable->RegisterCommand(_cmdName,                           \
                       static_cast<nsIControllerCommand *>(theCmd));

#define NS_REGISTER_NEXT_COMMAND(_cmdClass, _cmdName)                   \
    inCommandTable->RegisterCommand(_cmdName,                           \
                        static_cast<nsIControllerCommand *>(theCmd));

#define NS_REGISTER_LAST_COMMAND(_cmdClass, _cmdName)                   \
    inCommandTable->RegisterCommand(_cmdName,                           \
                       static_cast<nsIControllerCommand *>(theCmd));    \
  }

#define NS_REGISTER_STYLE_COMMAND(_cmdClass, _cmdName, _styleTag)       \
  {                                                                     \
    _cmdClass* theCmd = new _cmdClass(_styleTag);                       \
    inCommandTable->RegisterCommand(_cmdName,                           \
                       static_cast<nsIControllerCommand *>(theCmd));    \
  }

#define NS_REGISTER_TAG_COMMAND(_cmdClass, _cmdName, _tagName)          \
  {                                                                     \
    _cmdClass* theCmd = new _cmdClass(_tagName);                        \
    inCommandTable->RegisterCommand(_cmdName,                           \
                       static_cast<nsIControllerCommand *>(theCmd));    \
  }


// static
nsresult
HTMLEditorController::RegisterEditorDocStateCommands(
                        nsIControllerCommandTable *inCommandTable)
{
  // observer commands for document state
  NS_REGISTER_FIRST_COMMAND(DocumentStateCommand, "obs_documentCreated")
  NS_REGISTER_NEXT_COMMAND(DocumentStateCommand, "obs_documentWillBeDestroyed")
  NS_REGISTER_LAST_COMMAND(DocumentStateCommand, "obs_documentLocationChanged")

  // commands that may get or change state
  NS_REGISTER_FIRST_COMMAND(SetDocumentStateCommand, "cmd_setDocumentModified")
  NS_REGISTER_NEXT_COMMAND(SetDocumentStateCommand, "cmd_setDocumentUseCSS")
  NS_REGISTER_NEXT_COMMAND(SetDocumentStateCommand, "cmd_setDocumentReadOnly")
  NS_REGISTER_NEXT_COMMAND(SetDocumentStateCommand, "cmd_insertBrOnReturn")
  NS_REGISTER_NEXT_COMMAND(SetDocumentStateCommand, "cmd_defaultParagraphSeparator")
  NS_REGISTER_NEXT_COMMAND(SetDocumentStateCommand, "cmd_enableObjectResizing")
  NS_REGISTER_LAST_COMMAND(SetDocumentStateCommand, "cmd_enableInlineTableEditing")

  NS_REGISTER_ONE_COMMAND(SetDocumentOptionsCommand, "cmd_setDocumentOptions")

  return NS_OK;
}

// static
nsresult
HTMLEditorController::RegisterHTMLEditorCommands(
                        nsIControllerCommandTable *inCommandTable)
{
  // Edit menu
  NS_REGISTER_ONE_COMMAND(PasteNoFormattingCommand, "cmd_pasteNoFormatting");

  // indent/outdent
  NS_REGISTER_ONE_COMMAND(IndentCommand, "cmd_indent");
  NS_REGISTER_ONE_COMMAND(OutdentCommand, "cmd_outdent");

  // Styles
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_bold", nsGkAtoms::b);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_italic", nsGkAtoms::i);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_underline", nsGkAtoms::u);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_tt", nsGkAtoms::tt);

  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_strikethrough", nsGkAtoms::strike);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_superscript", nsGkAtoms::sup);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_subscript", nsGkAtoms::sub);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_nobreak", nsGkAtoms::nobr);

  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_em", nsGkAtoms::em);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_strong", nsGkAtoms::strong);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_cite", nsGkAtoms::cite);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_abbr", nsGkAtoms::abbr);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_acronym", nsGkAtoms::acronym);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_code", nsGkAtoms::code);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_samp", nsGkAtoms::samp);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_var", nsGkAtoms::var);
  NS_REGISTER_STYLE_COMMAND(StyleUpdatingCommand, "cmd_removeLinks", nsGkAtoms::href);

  // lists
  NS_REGISTER_STYLE_COMMAND(ListCommand,     "cmd_ol", nsGkAtoms::ol);
  NS_REGISTER_STYLE_COMMAND(ListCommand,     "cmd_ul", nsGkAtoms::ul);
  NS_REGISTER_STYLE_COMMAND(ListItemCommand, "cmd_dt", nsGkAtoms::dt);
  NS_REGISTER_STYLE_COMMAND(ListItemCommand, "cmd_dd", nsGkAtoms::dd);
  NS_REGISTER_ONE_COMMAND(RemoveListCommand, "cmd_removeList");

  // format stuff
  NS_REGISTER_ONE_COMMAND(ParagraphStateCommand,       "cmd_paragraphState");
  NS_REGISTER_ONE_COMMAND(FontFaceStateCommand,        "cmd_fontFace");
  NS_REGISTER_ONE_COMMAND(FontSizeStateCommand,        "cmd_fontSize");
  NS_REGISTER_ONE_COMMAND(FontColorStateCommand,       "cmd_fontColor");
  NS_REGISTER_ONE_COMMAND(BackgroundColorStateCommand, "cmd_backgroundColor");
  NS_REGISTER_ONE_COMMAND(HighlightColorStateCommand,  "cmd_highlight");

  NS_REGISTER_ONE_COMMAND(AlignCommand, "cmd_align");
  NS_REGISTER_ONE_COMMAND(RemoveStylesCommand, "cmd_removeStyles");

  NS_REGISTER_ONE_COMMAND(IncreaseFontSizeCommand, "cmd_increaseFont");
  NS_REGISTER_ONE_COMMAND(DecreaseFontSizeCommand, "cmd_decreaseFont");

  // Insert content
  NS_REGISTER_ONE_COMMAND(InsertHTMLCommand, "cmd_insertHTML");
  NS_REGISTER_TAG_COMMAND(InsertTagCommand, "cmd_insertLinkNoUI", nsGkAtoms::a);
  NS_REGISTER_TAG_COMMAND(InsertTagCommand, "cmd_insertImageNoUI", nsGkAtoms::img);
  NS_REGISTER_TAG_COMMAND(InsertTagCommand, "cmd_insertHR", nsGkAtoms::hr);

  NS_REGISTER_ONE_COMMAND(AbsolutePositioningCommand, "cmd_absPos");
  NS_REGISTER_ONE_COMMAND(DecreaseZIndexCommand, "cmd_decreaseZIndex");
  NS_REGISTER_ONE_COMMAND(IncreaseZIndexCommand, "cmd_increaseZIndex");

  return NS_OK;
}

} // namespace mozilla
