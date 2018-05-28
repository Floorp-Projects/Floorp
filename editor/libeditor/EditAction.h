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
  ignore = -1,

  // eNone indicates not edit sub-action is being handled.  This is useful
  // of initial value of member variables.
  eNone = 0,

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

  insertIMEText,
  deleteSelection,
  setTextProperty,
  removeTextProperty,
  outputText,
  setText,

  // HTML editor only actions
  insertBreak = 3000,
  makeList,
  indent,
  outdent,
  align,
  makeBasicBlock,
  removeList,
  makeDefListItem,
  insertElement,
  insertQuotation,

  htmlPaste = 3012,
  loadHTML,
  resetTextProperties,
  setAbsolutePosition,
  removeAbsolutePosition,
  decreaseZIndex,
  increaseZIndex,
};

} // namespace mozilla

inline bool operator!(const mozilla::EditSubAction& aEditSubAction)
{
  return aEditSubAction == mozilla::EditSubAction::eNone;
}

#endif // #ifdef mozilla_EditAction_h
