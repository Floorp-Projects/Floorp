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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Ryan Cassin <rcassin@supernova.org>
 *   Daniel Glazman <glazman@netscape.com>
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

#include "nsIControllerCommandTable.h"
#include "nsComposerController.h"
#include "nsComposerCommands.h"

#define NS_REGISTER_ONE_COMMAND(_cmdClass, _cmdName)                    \
  {                                                                     \
    _cmdClass* theCmd;                                                  \
    NS_NEWXPCOM(theCmd, _cmdClass);                                     \
    if (!theCmd) return NS_ERROR_OUT_OF_MEMORY;                         \
    rv = inCommandTable->RegisterCommand(_cmdName,                      \
                       NS_STATIC_CAST(nsIControllerCommand *, theCmd)); \
  }

#define NS_REGISTER_FIRST_COMMAND(_cmdClass, _cmdName)                  \
  {                                                                     \
    _cmdClass* theCmd;                                                  \
    NS_NEWXPCOM(theCmd, _cmdClass);                                     \
    if (!theCmd) return NS_ERROR_OUT_OF_MEMORY;                         \
    rv = inCommandTable->RegisterCommand(_cmdName,                      \
                       NS_STATIC_CAST(nsIControllerCommand *, theCmd));

#define NS_REGISTER_NEXT_COMMAND(_cmdClass, _cmdName)                   \
    rv = inCommandTable->RegisterCommand(_cmdName,                      \
                        NS_STATIC_CAST(nsIControllerCommand *, theCmd));

#define NS_REGISTER_LAST_COMMAND(_cmdClass, _cmdName)                   \
    rv = inCommandTable->RegisterCommand(_cmdName,                      \
                       NS_STATIC_CAST(nsIControllerCommand *, theCmd)); \
  }

#define NS_REGISTER_STYLE_COMMAND(_cmdClass, _cmdName, _styleTag)       \
  {                                                                     \
    _cmdClass* theCmd = new _cmdClass(_styleTag);                       \
    if (!theCmd) return NS_ERROR_OUT_OF_MEMORY;                         \
    rv = inCommandTable->RegisterCommand(_cmdName,                      \
                       NS_STATIC_CAST(nsIControllerCommand *, theCmd)); \
  }
  
#define NS_REGISTER_TAG_COMMAND(_cmdClass, _cmdName, _tagName)          \
  {                                                                     \
    _cmdClass* theCmd = new _cmdClass(_tagName);                        \
    if (!theCmd) return NS_ERROR_OUT_OF_MEMORY;                         \
    rv = inCommandTable->RegisterCommand(_cmdName,                      \
                       NS_STATIC_CAST(nsIControllerCommand *, theCmd)); \
  }
  

// static
nsresult
nsComposerController::RegisterEditorDocStateCommands(
                        nsIControllerCommandTable *inCommandTable)
{
  nsresult rv;

  // observer commands for document state
  NS_REGISTER_FIRST_COMMAND(nsDocumentStateCommand, "obs_documentCreated")
  NS_REGISTER_NEXT_COMMAND(nsDocumentStateCommand, "obs_documentWillBeDestroyed")
  NS_REGISTER_LAST_COMMAND(nsDocumentStateCommand, "obs_documentLocationChanged")

  // commands that may get or change state
  NS_REGISTER_FIRST_COMMAND(nsSetDocumentStateCommand, "cmd_setDocumentModified")
  NS_REGISTER_NEXT_COMMAND(nsSetDocumentStateCommand, "cmd_setDocumentUseCSS")
  NS_REGISTER_LAST_COMMAND(nsSetDocumentStateCommand, "cmd_setDocumentReadOnly")

  NS_REGISTER_ONE_COMMAND(nsSetDocumentOptionsCommand, "cmd_setDocumentOptions")

  return NS_OK;
}

// static
nsresult
nsComposerController::RegisterHTMLEditorCommands(
                        nsIControllerCommandTable *inCommandTable)
{
  nsresult rv;
  
  // Edit menu
  NS_REGISTER_ONE_COMMAND(nsPasteNoFormattingCommand, "cmd_pasteNoFormatting");

  // indent/outdent
  NS_REGISTER_ONE_COMMAND(nsIndentCommand, "cmd_indent");
  NS_REGISTER_ONE_COMMAND(nsOutdentCommand, "cmd_outdent");

  // Styles
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_bold", "b");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_italic", "i");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_underline", "u");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_tt", "tt");

  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_strikethrough", "strike");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_superscript", "sup");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_subscript", "sub");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_nobreak", "nobr");

  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_em", "em");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_strong", "strong");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_cite", "cite");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_abbr", "abbr");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_acronym", "acronym");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_code", "code");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_samp", "samp");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_var", "var");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_removeLinks", "href");

  // lists
  NS_REGISTER_STYLE_COMMAND(nsListCommand,     "cmd_ol", "ol");
  NS_REGISTER_STYLE_COMMAND(nsListCommand,     "cmd_ul", "ul");
  NS_REGISTER_STYLE_COMMAND(nsListItemCommand, "cmd_dt", "dt");
  NS_REGISTER_STYLE_COMMAND(nsListItemCommand, "cmd_dd", "dd");
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
  NS_REGISTER_TAG_COMMAND(nsInsertTagCommand, "cmd_insertLinkNoUI", "a");
  NS_REGISTER_TAG_COMMAND(nsInsertTagCommand, "cmd_insertImageNoUI", "img");
  NS_REGISTER_TAG_COMMAND(nsInsertTagCommand, "cmd_insertHR", "hr");

  NS_REGISTER_ONE_COMMAND(nsAbsolutePositioningCommand, "cmd_absPos");
  NS_REGISTER_ONE_COMMAND(nsDecreaseZIndexCommand, "cmd_decreaseZIndex");
  NS_REGISTER_ONE_COMMAND(nsIncreaseZIndexCommand, "cmd_increaseZIndex");

  return NS_OK;
}
