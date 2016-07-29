/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoRestyleManager_h
#define mozilla_ServoRestyleManager_h

#include "mozilla/EventStates.h"
#include "mozilla/RestyleManagerBase.h"
#include "mozilla/ServoElementSnapshot.h"
#include "nsChangeHint.h"
#include "nsHashKeys.h"
#include "nsINode.h"
#include "nsISupportsImpl.h"
#include "nsPresContext.h"

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla
class nsAttrValue;
class nsIAtom;
class nsIContent;
class nsIFrame;
class nsStyleChangeList;

namespace mozilla {

/**
 * Restyle manager for a Servo-backed style system.
 */
class ServoRestyleManager : public RestyleManagerBase
{
  friend class ServoStyleSet;
public:
  typedef RestyleManagerBase base_type;

  NS_INLINE_DECL_REFCOUNTING(ServoRestyleManager)

  explicit ServoRestyleManager(nsPresContext* aPresContext);

  void PostRestyleEvent(dom::Element* aElement,
                        nsRestyleHint aRestyleHint,
                        nsChangeHint aMinChangeHint);
  void PostRestyleEventForLazyConstruction();
  void RebuildAllStyleData(nsChangeHint aExtraHint,
                           nsRestyleHint aRestyleHint);
  void PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                    nsRestyleHint aRestyleHint);
  void ProcessPendingRestyles();
  void RestyleForInsertOrChange(dom::Element* aContainer,
                                nsIContent* aChild);
  void RestyleForAppend(dom::Element* aContainer,
                        nsIContent* aFirstNewContent);
  void RestyleForRemove(dom::Element* aContainer,
                        nsIContent* aOldChild,
                        nsIContent* aFollowingSibling);
  nsresult ContentStateChanged(nsIContent* aContent,
                               EventStates aStateMask);
  void AttributeWillChange(dom::Element* aElement,
                           int32_t aNameSpaceID,
                           nsIAtom* aAttribute,
                           int32_t aModType,
                           const nsAttrValue* aNewValue);

  // XXXbholley: We should assert that the element is already snapshotted.
  void AttributeChanged(dom::Element* aElement, int32_t aNameSpaceID,
                        nsIAtom* aAttribute, int32_t aModType,
                        const nsAttrValue* aOldValue) {}

  nsresult ReparentStyleContext(nsIFrame* aFrame);
  nsresult ProcessRestyledFrames(nsStyleChangeList& aChangeList);
  void FlushOverflowChangedTracker();

  bool HasPendingRestyles() { return !mModifiedElements.IsEmpty(); }

protected:
  ~ServoRestyleManager() {}

private:
  ServoElementSnapshot* SnapshotForElement(Element* aElement);

  /**
   * The element-to-element snapshot table to compute restyle hints.
   */
  nsClassHashtable<nsRefPtrHashKey<Element>, ServoElementSnapshot>
    mModifiedElements;

  /**
   * Traverses a tree of content that Servo has just restyled, recreating style
   * contexts for their frames with the new style data.
   */
  void RecreateStyleContexts(nsIContent* aContent,
                             nsStyleContext* aParentContext,
                             ServoStyleSet* aStyleSet,
                             nsStyleChangeList& aChangeList);

  /**
   * Marks the tree with the appropriate dirty flags for the given restyle hint.
   */
  static void NoteRestyleHint(Element* aElement, nsRestyleHint aRestyleHint);

  inline ServoStyleSet* StyleSet() const
  {
    MOZ_ASSERT(PresContext()->StyleSet()->IsServo(),
               "ServoRestyleManager should only be used with a Servo-flavored "
               "style backend");
    return PresContext()->StyleSet()->AsServo();
  }
};

} // namespace mozilla

#endif // mozilla_ServoRestyleManager_h
