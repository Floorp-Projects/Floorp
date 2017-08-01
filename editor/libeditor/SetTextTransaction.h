/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SetTextTransaction_h
#define mozilla_SetTextTransaction_h

#include "mozilla/Attributes.h"         // for MOZ_STACK_CLASS
#include "nsString.h"                   // nsString members
#include "nscore.h"                     // NS_IMETHOD, nsAString

namespace mozilla {

class EditorBase;
class RangeUpdater;

namespace dom {
class Text;
} // namespace dom

/**
 * A fake transaction that inserts text into a content node.
 *
 * This class mimics a transaction class but it is not intended to be used as one.
 */
class MOZ_STACK_CLASS SetTextTransaction final
{
public:
  /**
   * @param aTextNode       The text content node.
   * @param aString         The new text to insert.
   * @param aEditorBase     Used to get and set the selection.
   * @param aRangeUpdater   The range updater
   */
  SetTextTransaction(dom::Text& aTextNode,
                     const nsAString& aString, EditorBase& aEditorBase,
                     RangeUpdater* aRangeUpdater);

  nsresult DoTransaction();

private:
  // The Text node to operate upon.
  RefPtr<dom::Text> mTextNode;

  // The text to insert into mTextNode at mOffset.
  nsString mStringToSet;

  // The previous text for undo
  nsString mPreviousData;

  // The editor, which we'll need to get the selection.
  EditorBase* MOZ_NON_OWNING_REF mEditorBase;

  RangeUpdater* mRangeUpdater;
};

} // namespace mozilla

#endif // #ifndef mozilla_SetTextTransaction_h
