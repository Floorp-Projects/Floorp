/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EffectSet_h
#define mozilla_EffectSet_h

#include "mozilla/DebugOnly.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "nsHashKeys.h"  // For nsPtrHashKey
#include "nsTHashSet.h"

class nsPresContext;
enum class DisplayItemType : uint8_t;

namespace mozilla {

namespace dom {
class Element;
}  // namespace dom

enum class PseudoStyleType : uint8_t;

// A wrapper around a hashset of AnimationEffect objects to handle
// storing the set as a property of an element.
class EffectSet {
 public:
  EffectSet()
      : mCascadeNeedsUpdate(false),
        mAnimationGeneration(0)
#ifdef DEBUG
        ,
        mActiveIterators(0),
        mCalledPropertyDtor(false)
#endif
        ,
        mMayHaveOpacityAnim(false),
        mMayHaveTransformAnim(false) {
    MOZ_COUNT_CTOR(EffectSet);
  }

  ~EffectSet() {
    MOZ_ASSERT(mCalledPropertyDtor,
               "must call destructor through element property dtor");
    MOZ_ASSERT(mActiveIterators == 0,
               "Effect set should not be destroyed while it is being "
               "enumerated");
    MOZ_COUNT_DTOR(EffectSet);
  }
  static void PropertyDtor(void* aObject, nsAtom* aPropertyName,
                           void* aPropertyValue, void* aData);

  // Methods for supporting cycle-collection
  void Traverse(nsCycleCollectionTraversalCallback& aCallback);

  static EffectSet* GetEffectSet(const dom::Element* aElement,
                                 PseudoStyleType aPseudoType);
  static EffectSet* GetOrCreateEffectSet(dom::Element* aElement,
                                         PseudoStyleType aPseudoType);

  static EffectSet* GetEffectSetForFrame(const nsIFrame* aFrame,
                                         const nsCSSPropertyIDSet& aProperties);
  static EffectSet* GetEffectSetForFrame(const nsIFrame* aFrame,
                                         DisplayItemType aDisplayItemType);
  // Gets the EffectSet associated with the specified frame's content.
  //
  // Typically the specified frame should be a "style frame".
  //
  // That is because display:table content:
  //
  // - makes a distinction between the primary frame and style frame,
  // - associates the EffectSet with the style frame's content,
  // - applies transform animations to the primary frame.
  //
  // In such a situation, passing in the primary frame here will return nullptr
  // despite the fact that it has a transform animation applied to it.
  //
  // GetEffectSetForFrame, above, handles this by automatically looking up the
  // EffectSet on the corresponding style frame when querying transform
  // properties. Unless you are sure you know what you are doing, you should
  // try using GetEffectSetForFrame first.
  //
  // If you decide to use this, consider documenting why you are sure it is ok
  // to use this.
  static EffectSet* GetEffectSetForStyleFrame(const nsIFrame* aStyleFrame);

  static EffectSet* GetEffectSetForEffect(const dom::KeyframeEffect* aEffect);

  static void DestroyEffectSet(dom::Element* aElement,
                               PseudoStyleType aPseudoType);

  void AddEffect(dom::KeyframeEffect& aEffect);
  void RemoveEffect(dom::KeyframeEffect& aEffect);

  void SetMayHaveOpacityAnimation() { mMayHaveOpacityAnim = true; }
  bool MayHaveOpacityAnimation() const { return mMayHaveOpacityAnim; }
  void SetMayHaveTransformAnimation() { mMayHaveTransformAnim = true; }
  bool MayHaveTransformAnimation() const { return mMayHaveTransformAnim; }

 private:
  using OwningEffectSet = nsTHashSet<nsRefPtrHashKey<dom::KeyframeEffect>>;

 public:
  // A simple iterator to support iterating over the effects in this object in
  // range-based for loops.
  //
  // This allows us to avoid exposing mEffects directly and saves the
  // caller from having to dereference hashtable iterators using
  // the rather complicated: iter.Get()->GetKey().
  //
  // XXX Except for the active iterator checks, this could be replaced by the
  // STL-style iterators of nsTHashSet directly now.
  class Iterator {
   public:
    explicit Iterator(EffectSet& aEffectSet)
        : Iterator(aEffectSet, aEffectSet.mEffects.begin()) {}

    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator(Iterator&&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(Iterator&&) = delete;

    static Iterator EndIterator(EffectSet& aEffectSet) {
      return {aEffectSet, aEffectSet.mEffects.end()};
    }

#ifdef DEBUG
    ~Iterator() {
      MOZ_ASSERT(mEffectSet.mActiveIterators > 0);
      mEffectSet.mActiveIterators--;
    }
#endif

    bool operator!=(const Iterator& aOther) const {
      return mHashIterator != aOther.mHashIterator;
    }

    Iterator& operator++() {
      ++mHashIterator;
      return *this;
    }

    dom::KeyframeEffect* operator*() { return *mHashIterator; }

   private:
    Iterator(EffectSet& aEffectSet,
             OwningEffectSet::const_iterator aHashIterator)
        :
#ifdef DEBUG
          mEffectSet(aEffectSet),
#endif
          mHashIterator(std::move(aHashIterator)) {
#ifdef DEBUG
      mEffectSet.mActiveIterators++;
#endif
    }

#ifdef DEBUG
    EffectSet& mEffectSet;
#endif
    OwningEffectSet::const_iterator mHashIterator;
  };

  friend class Iterator;

  Iterator begin() { return Iterator(*this); }
  Iterator end() { return Iterator::EndIterator(*this); }
#ifdef DEBUG
  bool IsBeingEnumerated() const { return mActiveIterators != 0; }
#endif

  bool IsEmpty() const { return mEffects.IsEmpty(); }

  size_t Count() const { return mEffects.Count(); }

  const TimeStamp& LastOverflowAnimationSyncTime() const {
    return mLastOverflowAnimationSyncTime;
  }
  void UpdateLastOverflowAnimationSyncTime(const TimeStamp& aRefreshTime) {
    mLastOverflowAnimationSyncTime = aRefreshTime;
  }

  bool CascadeNeedsUpdate() const { return mCascadeNeedsUpdate; }
  void MarkCascadeNeedsUpdate() { mCascadeNeedsUpdate = true; }
  void MarkCascadeUpdated() { mCascadeNeedsUpdate = false; }

  void UpdateAnimationGeneration(nsPresContext* aPresContext);
  uint64_t GetAnimationGeneration() const { return mAnimationGeneration; }

  static nsAtom** GetEffectSetPropertyAtoms();

  const nsCSSPropertyIDSet& PropertiesWithImportantRules() const {
    return mPropertiesWithImportantRules;
  }
  nsCSSPropertyIDSet& PropertiesWithImportantRules() {
    return mPropertiesWithImportantRules;
  }
  nsCSSPropertyIDSet& PropertiesForAnimationsLevel() {
    return mPropertiesForAnimationsLevel;
  }
  nsCSSPropertyIDSet PropertiesForAnimationsLevel() const {
    return mPropertiesForAnimationsLevel;
  }

 private:
  static nsAtom* GetEffectSetPropertyAtom(PseudoStyleType aPseudoType);

  OwningEffectSet mEffects;

  // Refresh driver timestamp from the moment when the animations which produce
  // overflow change hints in this effect set were last updated.

  // This is used for animations whose main-thread restyling is throttled either
  // because they are running on the compositor or because they are not visible.
  // We still need to update them on the main thread periodically, however (e.g.
  // so scrollbars can be updated), so this tracks the last time we did that.
  TimeStamp mLastOverflowAnimationSyncTime;

  // Dirty flag to represent when the mPropertiesWithImportantRules and
  // mPropertiesForAnimationsLevel on effects in this set might need to be
  // updated.
  //
  // Set to true any time the set of effects is changed or when
  // one the effects goes in or out of the "in effect" state.
  bool mCascadeNeedsUpdate;

  // RestyleManager keeps track of the number of animation restyles.
  // 'mini-flushes' (see nsTransitionManager::UpdateAllThrottledStyles()).
  // mAnimationGeneration is the sequence number of the last flush where a
  // transition/animation changed.  We keep a similar count on the
  // corresponding layer so we can check that the layer is up to date with
  // the animation manager.
  uint64_t mAnimationGeneration;

  // Specifies the compositor-animatable properties that are overridden by
  // !important rules.
  nsCSSPropertyIDSet mPropertiesWithImportantRules;
  // Specifies the properties for which the result will be added to the
  // animations level of the cascade and hence should be skipped when we are
  // composing the animation style for the transitions level of the cascede.
  nsCSSPropertyIDSet mPropertiesForAnimationsLevel;

#ifdef DEBUG
  // Track how many iterators are referencing this effect set when we are
  // destroyed, we can assert that nothing is still pointing to us.
  uint64_t mActiveIterators;

  bool mCalledPropertyDtor;
#endif

  bool mMayHaveOpacityAnim;
  bool mMayHaveTransformAnim;
};

}  // namespace mozilla

#endif  // mozilla_EffectSet_h
