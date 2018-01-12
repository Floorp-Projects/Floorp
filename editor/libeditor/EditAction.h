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
enum class EditAction : int32_t
{
  ignore = -1,

  none = 0,
  undo,
  redo,
  insertNode,
  createNode,
  deleteNode,
  splitNode,
  joinNode,

  deleteText = 1003,

  // Text edit commands
  insertText = 2000,
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

inline bool operator!(const mozilla::EditAction& aOp)
{
  return aOp == mozilla::EditAction::none;
}

#endif // #ifdef mozilla_EditAction_h
