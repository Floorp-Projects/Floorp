/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditAction_h
#define mozilla_EditAction_h

namespace mozilla {

/**
 * EditAction indicates which operation or command causes running the methods
 * of editors.
 */
enum class EditAction
{
  // eNone indicates no edit action is being handled.
  eNone,

  // eNotEditing indicates that something is retrieved or initializing
  // something at creating, destroying or focus move etc, i.e., not edit
  // action is being handled but editor is doing something.
  eNotEditing,

  // eInsertText indicates to insert some characters.
  eInsertText,

  // eInsertParagraphSeparator indicates to insert a paragraph separator such
  // as <p>, <div> or just \n in TextEditor.
  eInsertParagraphSeparator,

  // eDeleteSelection indicates to delete selected content or content around
  // caret if selection is collapsed.
  eDeleteSelection,

  // eDeleteBackward indicates to remove previous character element of caret.
  // This may be set even when Selection is not collapsed.
  eDeleteBackward,

  // eDeleteForward indicates to remove next character or element of caret.
  // This may be set even when Selection is not collapsed.
  eDeleteForward,

  // eDeleteWordBackward indicates to remove previous word.  If caret is in
  // a word, remove characters between word start and caret.
  // This may be set even when Selection is not collapsed.
  eDeleteWordBackward,

  // eDeleteWordForward indicates to remove next word.  If caret is in a
  // word, remove characters between caret and word end.
  // This may be set even when Selection is not collapsed.
  eDeleteWordForward,

  // eDeleteToBeginningOfSoftLine indicates to remove characters between
  // caret and previous visual line break.
  // This may be set even when Selection is not collapsed.
  eDeleteToBeginningOfSoftLine,

  // eDeleteToEndOfSoftLine indicates to remove characters between caret and
  // next visual line break.
  // This may be set even when Selection is not collapsed.
  eDeleteToEndOfSoftLine,

  // eStartComposition indicates that user starts composition.
  eStartComposition,

  // eUpdateComposition indicates that user updates composition with
  // new composition string and IME selections.
  eUpdateComposition,

  // eCommitComposition indicates that user commits composition.
  eCommitComposition,

  // eEndComposition indicates that user ends composition.
  eEndComposition,

  // eUndo/eRedo indicate to undo/redo a transaction.
  eUndo,
  eRedo,

  // eSetTextDirection indicates that setting text direction (LTR or RTL).
  eSetTextDirection,

  // eCut indicates to delete selected content and copy it to the clipboard.
  eCut,

  // eCopy indicates to copy selected content to the clipboard.
  eCopy,

  // ePaste indicates to paste clipboard data.
  ePaste,

  // eDrop indicates that user drops dragging item into the editor.
  eDrop,

  // eReplaceText indicates to replace a part of range in editor with
  // specific text.  For example, user select a correct word in suggestions
  // of spellchecker or a suggestion in list of autocomplete.
  eReplaceText,

  // The following edit actions are not user's operation.  They are caused
  // by if UI does something or web apps does something with JS.

  // eUnknown indicates some special edit actions, e.g., batching of some
  // nsI*Editor method calls.  This shouldn't be set while handling a user
  // operation.
  eUnknown,

  // eSetAttribute indicates to set attribute value of an element node.
  eSetAttribute,

  // eRemoveAttribute indicates to remove attribute from an element node.
  eRemoveAttribute,

  // eInsertNode indicates to insert a node into the tree.
  eInsertNode,

  // eDeleteNode indicates to remove a node form the tree.
  eRemoveNode,

  // eSplitNode indicates to split a node.
  eSplitNode,

  // eJoinNodes indicates to join 2 nodes.
  eJoinNodes,

  // eSetCharacterSet indicates to set character-set of the document.
  eSetCharacterSet,

  // eSetWrapWidth indicates to set wrap width.
  eSetWrapWidth,

  // eSetText indicates to set new text of TextEditor, e.g., setting
  // HTMLInputElement.value.
  eSetText,

  // eHidePassword indicates that editor hides password with mask characters.
  eHidePassword,
};

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
