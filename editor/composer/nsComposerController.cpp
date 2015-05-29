/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc.h"           // for operator new
#include "nsComposerCommands.h"         // for nsStyleUpdatingCommand, etc
#include "nsComposerController.h"
#include "nsError.h"                    // for NS_OK
#include "nsGkAtoms.h"                  // for nsGkAtoms, nsGkAtoms::a, etc
#include "nsIControllerCommandTable.h"  // for nsIControllerCommandTable

class nsIControllerCommand;

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
nsComposerController::RegisterEditorDocStateCommands(
                        nsIControllerCommandTable *inCommandTable)
{
  // observer commands for document state
  NS_REGISTER_FIRST_COMMAND(nsDocumentStateCommand, "obs_documentCreated")
  NS_REGISTER_NEXT_COMMAND(nsDocumentStateCommand, "obs_documentWillBeDestroyed")
  NS_REGISTER_LAST_COMMAND(nsDocumentStateCommand, "obs_documentLocationChanged")

  // commands that may get or change state
  NS_REGISTER_FIRST_COMMAND(nsSetDocumentStateCommand, "cmd_setDocumentModified")
  NS_REGISTER_NEXT_COMMAND(nsSetDocumentStateCommand, "cmd_setDocumentUseCSS")
  NS_REGISTER_NEXT_COMMAND(nsSetDocumentStateCommand, "cmd_setDocumentReadOnly")
  NS_REGISTER_NEXT_COMMAND(nsSetDocumentStateCommand, "cmd_insertBrOnReturn")
  NS_REGISTER_NEXT_COMMAND(nsSetDocumentStateCommand, "cmd_enableObjectResizing")
  NS_REGISTER_LAST_COMMAND(nsSetDocumentStateCommand, "cmd_enableInlineTableEditing")

  NS_REGISTER_ONE_COMMAND(nsSetDocumentOptionsCommand, "cmd_setDocumentOptions")

  return NS_OK;
}

// static
nsresult
nsComposerController::RegisterHTMLEditorCommands(
                        nsIControllerCommandTable *inCommandTable)
{
  // Edit menu
  NS_REGISTER_ONE_COMMAND(nsPasteNoFormattingCommand, "cmd_pasteNoFormatting");

  // indent/outdent
  NS_REGISTER_ONE_COMMAND(nsIndentCommand, "cmd_indent");
  NS_REGISTER_ONE_COMMAND(nsOutdentCommand, "cmd_outdent");

  // Styles
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_bold", nsGkAtoms::b);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_italic", nsGkAtoms::i);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_underline", nsGkAtoms::u);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_tt", nsGkAtoms::tt);

  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_strikethrough", nsGkAtoms::strike);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_superscript", nsGkAtoms::sup);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_subscript", nsGkAtoms::sub);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_nobreak", nsGkAtoms::nobr);

  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_em", nsGkAtoms::em);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_strong", nsGkAtoms::strong);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_cite", nsGkAtoms::cite);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_abbr", nsGkAtoms::abbr);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_acronym", nsGkAtoms::acronym);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_code", nsGkAtoms::code);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_samp", nsGkAtoms::samp);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_var", nsGkAtoms::var);
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_removeLinks", nsGkAtoms::href);

  // lists
  NS_REGISTER_STYLE_COMMAND(nsListCommand,     "cmd_ol", nsGkAtoms::ol);
  NS_REGISTER_STYLE_COMMAND(nsListCommand,     "cmd_ul", nsGkAtoms::ul);
  NS_REGISTER_STYLE_COMMAND(nsListItemCommand, "cmd_dt", nsGkAtoms::dt);
  NS_REGISTER_STYLE_COMMAND(nsListItemCommand, "cmd_dd", nsGkAtoms::dd);
  NS_REGISTER_ONE_COMMAND(nsRemoveListCommand, "cmd_removeList");

  // format stuff
  NS_REGISTER_ONE_COMMAND(nsParagraphStateCommand,       "cmd_paragraphState");
  NS_REGISTER_ONE_COMMAND(nsFontFaceStateCommand,        "cmd_fontFace");
  NS_REGISTER_ONE_COMMAND(nsFontSizeStateCommand,        "cmd_fontSize");
  NS_REGISTER_ONE_COMMAND(nsFontColorStateCommand,       "cmd_fontColor");
  NS_REGISTER_ONE_COMMAND(nsBackgroundColorStateCommand, "cmd_backgroundColor");
  NS_REGISTER_ONE_COMMAND(nsHighlightColorStateCommand,  "cmd_highlight");

  NS_REGISTER_ONE_COMMAND(nsAlignCommand, "cmd_align");
  NS_REGISTER_ONE_COMMAND(nsRemoveStylesCommand, "cmd_removeStyles");

  NS_REGISTER_ONE_COMMAND(nsIncreaseFontSizeCommand, "cmd_increaseFont");
  NS_REGISTER_ONE_COMMAND(nsDecreaseFontSizeCommand, "cmd_decreaseFont");

  // Insert content
  NS_REGISTER_ONE_COMMAND(nsInsertHTMLCommand, "cmd_insertHTML");
  NS_REGISTER_TAG_COMMAND(nsInsertTagCommand, "cmd_insertLinkNoUI", nsGkAtoms::a);
  NS_REGISTER_TAG_COMMAND(nsInsertTagCommand, "cmd_insertImageNoUI", nsGkAtoms::img);
  NS_REGISTER_TAG_COMMAND(nsInsertTagCommand, "cmd_insertHR", nsGkAtoms::hr);

  NS_REGISTER_ONE_COMMAND(nsAbsolutePositioningCommand, "cmd_absPos");
  NS_REGISTER_ONE_COMMAND(nsDecreaseZIndexCommand, "cmd_decreaseZIndex");
  NS_REGISTER_ONE_COMMAND(nsIncreaseZIndexCommand, "cmd_increaseZIndex");

  return NS_OK;
}
