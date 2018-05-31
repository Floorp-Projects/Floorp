/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditAction_h
#define mozilla_EditAction_h

namespace mozilla {

// This is int32_t instead of int16_t because nsIInlineSpellChecker.idl's
// spellCheckAfterEditorChange is defined to take it as a long.
// TODO: Make each name eFoo and investigate whether the numeric values
//       still have some meaning.
enum class EditSubAction : int32_t
{
  // eNone indicates not edit sub-action is being handled.  This is useful
  // of initial value of member variables.
  eNone,

  // eUndo and eRedo indicate entire actions of undo/redo operation.
  eUndo,
  eRedo,

  // eInsertNode indicates to insert a new node into the DOM tree.
  eInsertNode,

  // eCreateNode indicates to create a new node and insert it into the DOM tree.
  eCreateNode,

  // eDeleteNode indicates to remove a node from the DOM tree.
  eDeleteNode,

  // eSplitNode indicates to split a node to 2 nodes.
  eSplitNode,

  // eJoinNodes indicates to join 2 nodes.
  eJoinNodes,

  // eDeleteText indicates to delete some characters form a text node.
  eDeleteText,

  // eInsertText indicates to insert some characters.
  eInsertText,

  // eInsertTextComingFromIME indicates to insert or update composition string
  // with new text which is new composition string or commit string.
  eInsertTextComingFromIME,

  // eDeleteSelectedContent indicates to remove selected content.
  eDeleteSelectedContent,

  // eSetTextProperty indicates to set a style from text.
  eSetTextProperty,

  // eRemoveTextProperty indicates to remove a style from text.
  eRemoveTextProperty,

  // eRemoveAllTextProperties indicate to remove all styles from text.
  eRemoveAllTextProperties,

  // eComputeTextToOutput indicates to compute the editor value as plain text
  // or something requested format.
  eComputeTextToOutput,

  // eSetText indicates to set editor value to new value.
  eSetText,

  // eInsertParagraphSeparator indicates to insert paragraph separator, <br> or
  // \n at least to break current line.
  eInsertParagraphSeparator,

  // eCreateOrChangeList indicates to create new list or change existing list
  // type.
  eCreateOrChangeList,

  // eIndent and eOutdent indicates to indent or outdent the target with
  // using <blockquote>, <ul>, <ol> or just margin of start edge.
  eIndent,
  eOutdent,

  // eSetOrClearAlignment aligns content or clears alignment with align
  // attribute or text-align.
  eSetOrClearAlignment,

  // eCreateOrRemoveBlock creates new block or removes existing block and
  // move its descendants to where the block was.
  eCreateOrRemoveBlock,

  // eRemoveList removes specific type of list but keep its content.
  eRemoveList,

  // eCreateOrChangeDefinitionList indicates to create new definition list or
  // change existing list to a definition list.
  eCreateOrChangeDefinitionList,

  // eInsertElement indicates to insert an element.
  eInsertElement,

  // eInsertQuotation indicates to insert an element and make it "quoted text".
  eInsertQuotation,

  // ePasteHTMLContent indicates to paste HTML content in clipboard.
  ePasteHTMLContent,

  // eInsertHTMLSource indicates to create a document fragment from given HTML
  // source and insert into the DOM tree.  So, this is similar to innerHTML.
  eInsertHTMLSource,

  // eReplaceHeadWithHTMLSource indicates to create a document fragment from
  // given HTML source and replace content of <head> with it.
  eReplaceHeadWithHTMLSource,

  // eSetPositionToAbsolute and eSetPositionToStatic indicates to set position
  // property to absolute or static.
  eSetPositionToAbsolute,
  eSetPositionToStatic,

  // eDecreaseZIndex and eIncreaseZIndex indicate to decrease and increase
  // z-index value.
  eDecreaseZIndex,
  eIncreaseZIndex,

  // eCreateBogusNode indicates to create a bogus <br> node.
  eCreateBogusNode,
};

} // namespace mozilla

inline bool operator!(const mozilla::EditSubAction& aEditSubAction)
{
  return aEditSubAction == mozilla::EditSubAction::eNone;
}

#endif // #ifdef mozilla_EditAction_h
