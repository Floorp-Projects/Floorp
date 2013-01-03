/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMCaretPosition_h
#define nsDOMCaretPosition_h

#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsINode.h"
#include "nsWrapperCache.h"

/**
 * Implementation of a DOM Caret Position, which is a node and offset within
 * that node, in the DOM tree.
 *
 * http://www.w3.org/TR/cssom-view/#dom-documentview-caretrangefrompoint
 *
 * @see Document::caretPositionFromPoint(float x, float y)
 */
class nsDOMCaretPosition : public nsISupports,
                           public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMCaretPosition)

  nsDOMCaretPosition(nsINode* aNode, uint32_t aOffset);

  /**
   * Retrieve the offset (character position within the DOM node) of the
   * CaretPosition.
   *
   * @returns The offset within the DOM node.
   */
  uint32_t Offset() const { return mOffset; }

  /**
   * Retrieve the DOM node with which this CaretPosition was established.
   * Normally, this will be created from a point, so it will be the DOM
   * node that lies at the point specified.
   *
   * @returns The DOM node of the CaretPosition.
   *
   * @see Document::caretPositionFromPoint(float x, float y)
   */
  nsINode* GetOffsetNode() const;

  nsISupports* GetParentObject() const
  {
    return GetOffsetNode();
  }

  virtual JSObject* WrapObject(JSContext *aCx, JSObject *aScope, bool *aTried)
    MOZ_OVERRIDE MOZ_FINAL;

protected:
  virtual ~nsDOMCaretPosition();
  uint32_t mOffset;
  nsCOMPtr<nsINode> mOffsetNode;
};
#endif

