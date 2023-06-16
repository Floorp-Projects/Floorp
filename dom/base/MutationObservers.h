/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_BASE_MUTATIONOBSERVERS_H_
#define DOM_BASE_MUTATIONOBSERVERS_H_

#include "mozilla/DoublyLinkedList.h"
#include "nsIContent.h"  // for use in inline function (NotifyParentChainChanged)
#include "nsIMutationObserver.h"  // for use in inline function (NotifyParentChainChanged)
#include "nsINode.h"

class nsAtom;
class nsAttrValue;

namespace mozilla::dom {
class Animation;
class Element;

class MutationObservers {
 public:
  /**
   * Send CharacterDataWillChange notifications to nsIMutationObservers.
   * @param aContent  Node whose data changed
   * @param aInfo     Struct with information details about the change
   * @see nsIMutationObserver::CharacterDataWillChange
   */
  static void NotifyCharacterDataWillChange(nsIContent* aContent,
                                            const CharacterDataChangeInfo&);

  /**
   * Send CharacterDataChanged notifications to nsIMutationObservers.
   * @param aContent  Node whose data changed
   * @param aInfo     Struct with information details about the change
   * @see nsIMutationObserver::CharacterDataChanged
   */
  static void NotifyCharacterDataChanged(nsIContent* aContent,
                                         const CharacterDataChangeInfo&);

  /**
   * Send AttributeWillChange notifications to nsIMutationObservers.
   * @param aElement      Element whose data will change
   * @param aNameSpaceID  Namespace of changing attribute
   * @param aAttribute    Local-name of changing attribute
   * @param aModType      Type of change (add/change/removal)
   * @see nsIMutationObserver::AttributeWillChange
   */
  static void NotifyAttributeWillChange(mozilla::dom::Element* aElement,
                                        int32_t aNameSpaceID,
                                        nsAtom* aAttribute, int32_t aModType);

  /**
   * Send AttributeChanged notifications to nsIMutationObservers.
   * @param aElement      Element whose data changed
   * @param aNameSpaceID  Namespace of changed attribute
   * @param aAttribute    Local-name of changed attribute
   * @param aModType      Type of change (add/change/removal)
   * @param aOldValue     If the old value was StoresOwnData() (or absent),
   *                      that value, otherwise null
   * @see nsIMutationObserver::AttributeChanged
   */
  static void NotifyAttributeChanged(mozilla::dom::Element* aElement,
                                     int32_t aNameSpaceID, nsAtom* aAttribute,
                                     int32_t aModType,
                                     const nsAttrValue* aOldValue);

  /**
   * Send AttributeSetToCurrentValue notifications to nsIMutationObservers.
   * @param aElement      Element whose data changed
   * @param aNameSpaceID  Namespace of the attribute
   * @param aAttribute    Local-name of the attribute
   * @see nsIMutationObserver::AttributeSetToCurrentValue
   */
  static void NotifyAttributeSetToCurrentValue(mozilla::dom::Element* aElement,
                                               int32_t aNameSpaceID,
                                               nsAtom* aAttribute);

  /**
   * Send ContentAppended notifications to nsIMutationObservers
   * @param aContainer           Node into which new child/children were added
   * @param aFirstNewContent     First new child
   * @see nsIMutationObserver::ContentAppended
   */
  static void NotifyContentAppended(nsIContent* aContainer,
                                    nsIContent* aFirstNewContent);

  /**
   * Send ContentInserted notifications to nsIMutationObservers
   * @param aContainer        Node into which new child was inserted
   * @param aChild            Newly inserted child
   * @see nsIMutationObserver::ContentInserted
   */
  static void NotifyContentInserted(nsINode* aContainer, nsIContent* aChild);
  /**
   * Send ContentRemoved notifications to nsIMutationObservers
   * @param aContainer        Node from which child was removed
   * @param aChild            Removed child
   * @param aPreviousSibling  Previous sibling of the removed child
   * @see nsIMutationObserver::ContentRemoved
   */
  static void NotifyContentRemoved(nsINode* aContainer, nsIContent* aChild,
                                   nsIContent* aPreviousSibling);

  /**
   * Send ParentChainChanged notifications to nsIMutationObservers
   * @param aContent  The piece of content that had its parent changed.
   * @see nsIMutationObserver::ParentChainChanged
   */
  static inline void NotifyParentChainChanged(nsIContent* aContent) {
    mozilla::SafeDoublyLinkedList<nsIMutationObserver>* observers =
        aContent->GetMutationObservers();
    if (observers) {
      for (auto iter = observers->begin(); iter != observers->end(); ++iter) {
        if (iter->IsCallbackEnabled(nsIMutationObserver::kParentChainChanged)) {
          iter->ParentChainChanged(aContent);
        }
      }
    }
  }

  static void NotifyARIAAttributeDefaultWillChange(
      mozilla::dom::Element* aElement, nsAtom* aAttribute, int32_t aModType);
  static void NotifyARIAAttributeDefaultChanged(mozilla::dom::Element* aElement,
                                                nsAtom* aAttribute,
                                                int32_t aModType);

  /**
   * Notify that an animation is added/changed/removed.
   * @param aAnimation The animation we added/changed/removed.
   */
  static void NotifyAnimationAdded(mozilla::dom::Animation* aAnimation);
  static void NotifyAnimationChanged(mozilla::dom::Animation* aAnimation);
  static void NotifyAnimationRemoved(mozilla::dom::Animation* aAnimation);

 private:
  enum class AnimationMutationType { Added, Changed, Removed };
  /**
   * Notify the observers of the target of an animation
   * @param aAnimation The mutated animation.
   * @param aMutationType The mutation type of this animation. It could be
   *                      Added, Changed, or Removed.
   */
  static void NotifyAnimationMutated(mozilla::dom::Animation* aAnimation,
                                     AnimationMutationType aMutatedType);
};
}  // namespace mozilla::dom

#endif  // DOM_BASE_MUTATIONOBSERVERS_H_
