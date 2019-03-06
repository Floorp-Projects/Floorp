/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This header file defines all inputType values which are used for DOM
 * InputEvent.inputType.
 * You must define NS_DEFINE_INPUTTYPE macro before including this.
 *
 * It must have two arguments, (aCPPName, aDOMName)
 * aCPPName is usable name for a part of C++ constants.
 * aDOMName is the actual value declared by the specs:
 * Level 1:
 *   https://rawgit.com/w3c/input-events/v1/index.html#interface-InputEvent-Attributes
 * Level 2:
 *   https://w3c.github.io/input-events/index.html#interface-InputEvent-Attributes
 */

NS_DEFINE_INPUTTYPE(InsertText, "insertText")
NS_DEFINE_INPUTTYPE(InsertReplacementText, "insertReplacementText")
NS_DEFINE_INPUTTYPE(InsertLineBreak, "insertLineBreak")
NS_DEFINE_INPUTTYPE(InsertParagraph, "insertParagraph")
NS_DEFINE_INPUTTYPE(InsertOrderedList, "insertOrderedList")
NS_DEFINE_INPUTTYPE(InsertUnorderedList, "insertUnorderedList")
NS_DEFINE_INPUTTYPE(InsertHorizontalRule, "insertHorizontalRule")
NS_DEFINE_INPUTTYPE(InsertFromYank, "insertFromYank")
NS_DEFINE_INPUTTYPE(InsertFromDrop, "insertFromDrop")
NS_DEFINE_INPUTTYPE(InsertFromPaste, "insertFromPaste")
NS_DEFINE_INPUTTYPE(InsertFromPasteAsQuotation, "insertFromPasteAsQuotation")
NS_DEFINE_INPUTTYPE(InsertTranspose, "insertTranspose")
NS_DEFINE_INPUTTYPE(InsertCompositionText, "insertCompositionText")
NS_DEFINE_INPUTTYPE(InsertFromComposition,
                    "insertFromComposition")  // Level 2
NS_DEFINE_INPUTTYPE(InsertLink, "insertLink")
NS_DEFINE_INPUTTYPE(DeleteByComposition,
                    "deleteByComposition")  // Level 2
NS_DEFINE_INPUTTYPE(DeleteCompositionText,
                    "deleteCompositionText")  // Level 2
NS_DEFINE_INPUTTYPE(DeleteWordBackward, "deleteWordBackward")
NS_DEFINE_INPUTTYPE(DeleteWordForward, "deleteWordForward")
NS_DEFINE_INPUTTYPE(DeleteSoftLineBackward, "deleteSoftLineBackward")
NS_DEFINE_INPUTTYPE(DeleteSoftLineForward, "deleteSoftLineForward")
NS_DEFINE_INPUTTYPE(DeleteEntireSoftLine, "deleteEntireSoftLine")
NS_DEFINE_INPUTTYPE(DeleteHardLineBackward, "deleteHardLineBackward")
NS_DEFINE_INPUTTYPE(DeleteHardLineForward, "deleteHardLineForward")
NS_DEFINE_INPUTTYPE(DeleteByDrag, "deleteByDrag")
NS_DEFINE_INPUTTYPE(DeleteByCut, "deleteByCut")
NS_DEFINE_INPUTTYPE(DeleteContent, "deleteContent")
NS_DEFINE_INPUTTYPE(DeleteContentBackward, "deleteContentBackward")
NS_DEFINE_INPUTTYPE(DeleteContentForward, "deleteContentForward")
NS_DEFINE_INPUTTYPE(HistoryUndo, "historyUndo")
NS_DEFINE_INPUTTYPE(HistoryRedo, "historyRedo")
NS_DEFINE_INPUTTYPE(FormatBold, "formatBold")
NS_DEFINE_INPUTTYPE(FormatItalic, "formatItalic")
NS_DEFINE_INPUTTYPE(FormatUnderline, "formatUnderline")
NS_DEFINE_INPUTTYPE(FormatStrikeThrough, "formatStrikeThrough")
NS_DEFINE_INPUTTYPE(FormatSuperscript, "formatSuperscript")
NS_DEFINE_INPUTTYPE(FormatSubscript, "formatSubscript")
NS_DEFINE_INPUTTYPE(FormatJustifyFull, "formatJustifyFull")
NS_DEFINE_INPUTTYPE(FormatJustifyCenter, "formatJustifyCenter")
NS_DEFINE_INPUTTYPE(FormatJustifyRight, "formatJustifyRight")
NS_DEFINE_INPUTTYPE(FormatJustifyLeft, "formatJustifyLeft")
NS_DEFINE_INPUTTYPE(FormatIndent, "formatIndent")
NS_DEFINE_INPUTTYPE(FormatOutdent, "formatOutdent")
NS_DEFINE_INPUTTYPE(FormatRemove, "formatRemove")
NS_DEFINE_INPUTTYPE(FormatSetBlockTextDirection, "formatSetBlockTextDirection")
NS_DEFINE_INPUTTYPE(FormatSetInlineTextDirection,
                    "formatSetInlineTextDirection")
NS_DEFINE_INPUTTYPE(FormatBackColor, "formatBackColor")
NS_DEFINE_INPUTTYPE(FormatFontColor, "formatFontColor")
NS_DEFINE_INPUTTYPE(FormatFontName, "formatFontName")
