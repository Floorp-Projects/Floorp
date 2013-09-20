/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMCaretPosition_h
#define nsDOMCaretPosition_h

#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsINode.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {
class DOMRect;
}
}

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
  typedef mozilla::dom::DOMRect DOMRect;

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

  /**
   * Retrieve the bounding rectangle of this CaretPosition object.
   *
   * @returns An nsClientRect representing the bounding rectangle of this
   *          CaretPosition, if one can be successfully determined, otherwise
   *          nullptr.
   */
  already_AddRefed<DOMRect> GetClientRect() const;

  /**
   * Set the anonymous content node that is the actual parent of this
   * CaretPosition object. In situations where the DOM node for a CaretPosition
   * actually lies within an anonymous content node (e.g. a textarea), the
   * actual parent is not set as the offset node. This is used to get the
   * correct bounding box of a CaretPosition object that lies within a textarea
   * or input element.
   *
   * @param aNode A pointer to an nsINode object that is the actual element
   *        within which this CaretPosition lies, but is an anonymous content
   *        node.
   */
  void SetAnonymousContentNode(nsINode* aNode)
  {
    mAnonymousContentNode = aNode;
  }

  nsISupports* GetParentObject() const
  {
    return GetOffsetNode();
  }

  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aScope)
    MOZ_OVERRIDE MOZ_FINAL;

protected:
  virtual ~nsDOMCaretPosition();

  uint32_t mOffset;
  nsCOMPtr<nsINode> mOffsetNode;
  nsCOMPtr<nsINode> mAnonymousContentNode;
};
#endif

