/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoRestyleManager_h
#define mozilla_ServoRestyleManager_h

#include "mozilla/EventStates.h"
#include "mozilla/RestyleManagerBase.h"
#include "nsChangeHint.h"
#include "nsISupportsImpl.h"
#include "nsPresContext.h"
#include "nsINode.h"

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla
class nsAttrValue;
class nsIAtom;
class nsIContent;
class nsIFrame;

namespace mozilla {

/**
 * Restyle manager for a Servo-backed style system.
 */
class ServoRestyleManager : public RestyleManagerBase
{
  friend class ServoStyleSet;
public:
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
  void AttributeChanged(dom::Element* aElement,
                        int32_t aNameSpaceID,
                        nsIAtom* aAttribute,
                        int32_t aModType,
                        const nsAttrValue* aOldValue);
  nsresult ReparentStyleContext(nsIFrame* aFrame);

  bool HasPendingRestyles() {
    if (MOZ_UNLIKELY(IsDisconnected())) {
      return false;
    }

    return PresContext()->PresShell()->GetDocument()->HasDirtyDescendantsForServo();
  }

protected:
  ~ServoRestyleManager() {}

private:
  /**
   * Traverses a tree of content that Servo has just restyled, recreating style
   * contexts for their frames with the new style data.
   *
   * This is just static so ServoStyleSet can mark this class as friend, so we
   * can access to the GetContext method without making it available to everyone
   * else.
   */
  static void RecreateStyleContexts(nsIContent* aContent,
                                    nsStyleContext* aParentContext,
                                    ServoStyleSet* aStyleSet);

  /**
   * Propagates the IS_DIRTY flag down to the tree, setting
   * HAS_DIRTY_DESCENDANTS appropriately.
   */
  static void DirtyTree(nsIContent* aContent);

  inline ServoStyleSet* StyleSet() const {
    MOZ_ASSERT(PresContext()->StyleSet()->IsServo(),
               "ServoRestyleManager should only be used with a Servo-flavored "
               "style backend");
    return PresContext()->StyleSet()->AsServo();
  }
};

} // namespace mozilla

#endif // mozilla_ServoRestyleManager_h
