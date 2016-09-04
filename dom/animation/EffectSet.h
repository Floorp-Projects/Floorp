/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EffectSet_h
#define mozilla_EffectSet_h

#include "mozilla/AnimValuesStyleRule.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/KeyframeEffectReadOnly.h"
#include "nsHashKeys.h" // For nsPtrHashKey
#include "nsTHashtable.h" // For nsTHashtable

class nsPresContext;

namespace mozilla {

namespace dom {
class Element;
} // namespace dom

enum class CSSPseudoElementType : uint8_t;

// A wrapper around a hashset of AnimationEffect objects to handle
// storing the set as a property of an element.
class EffectSet
{
public:
  EffectSet()
    : mCascadeNeedsUpdate(false)
    , mAnimationGeneration(0)
#ifdef DEBUG
    , mActiveIterators(0)
    , mCalledPropertyDtor(false)
#endif
  {
    MOZ_COUNT_CTOR(EffectSet);
  }

  ~EffectSet()
  {
    MOZ_ASSERT(mCalledPropertyDtor,
               "must call destructor through element property dtor");
    MOZ_ASSERT(mActiveIterators == 0,
               "Effect set should not be destroyed while it is being "
               "enumerated");
    MOZ_COUNT_DTOR(EffectSet);
  }
  static void PropertyDtor(void* aObject, nsIAtom* aPropertyName,
                           void* aPropertyValue, void* aData);

  // Methods for supporting cycle-collection
  void Traverse(nsCycleCollectionTraversalCallback& aCallback);

  static EffectSet* GetEffectSet(dom::Element* aElement,
                                 CSSPseudoElementType aPseudoType);
  static EffectSet* GetEffectSet(const nsIFrame* aFrame);
  static EffectSet* GetOrCreateEffectSet(dom::Element* aElement,
                                         CSSPseudoElementType aPseudoType);
  static void DestroyEffectSet(dom::Element* aElement,
                               CSSPseudoElementType aPseudoType);

  void AddEffect(dom::KeyframeEffectReadOnly& aEffect);
  void RemoveEffect(dom::KeyframeEffectReadOnly& aEffect);

private:
  typedef nsTHashtable<nsRefPtrHashKey<dom::KeyframeEffectReadOnly>>
    OwningEffectSet;

public:
  // A simple iterator to support iterating over the effects in this object in
  // range-based for loops.
  //
  // This allows us to avoid exposing mEffects directly and saves the
  // caller from having to dereference hashtable iterators using
  // the rather complicated: iter.Get()->GetKey().
  class Iterator
  {
  public:
    explicit Iterator(EffectSet& aEffectSet)
      : mEffectSet(aEffectSet)
      , mHashIterator(mozilla::Move(aEffectSet.mEffects.Iter()))
      , mIsEndIterator(false)
    {
#ifdef DEBUG
      mEffectSet.mActiveIterators++;
#endif
    }

    Iterator(Iterator&& aOther)
      : mEffectSet(aOther.mEffectSet)
      , mHashIterator(mozilla::Move(aOther.mHashIterator))
      , mIsEndIterator(aOther.mIsEndIterator)
    {
#ifdef DEBUG
      mEffectSet.mActiveIterators++;
#endif
    }

    static Iterator EndIterator(EffectSet& aEffectSet)
    {
      Iterator result(aEffectSet);
      result.mIsEndIterator = true;
      return result;
    }

    ~Iterator()
    {
#ifdef DEBUG
      MOZ_ASSERT(mEffectSet.mActiveIterators > 0);
      mEffectSet.mActiveIterators--;
#endif
    }

    bool operator!=(const Iterator& aOther) const {
      if (Done() || aOther.Done()) {
        return Done() != aOther.Done();
      }
      return mHashIterator.Get() != aOther.mHashIterator.Get();
    }

    Iterator& operator++() {
      MOZ_ASSERT(!Done());
      mHashIterator.Next();
      return *this;
    }

    dom::KeyframeEffectReadOnly* operator* ()
    {
      MOZ_ASSERT(!Done());
      return mHashIterator.Get()->GetKey();
    }

  private:
    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(const Iterator&&) = delete;

    bool Done() const {
      return mIsEndIterator || mHashIterator.Done();
    }

    EffectSet& mEffectSet;
    OwningEffectSet::Iterator mHashIterator;
    bool mIsEndIterator;
  };

  friend class Iterator;

  Iterator begin() { return Iterator(*this); }
  Iterator end() { return Iterator::EndIterator(*this); }
#ifdef DEBUG
  bool IsBeingEnumerated() const { return mActiveIterators != 0; }
#endif

  bool IsEmpty() const { return mEffects.IsEmpty(); }

  RefPtr<AnimValuesStyleRule>& AnimationRule(EffectCompositor::CascadeLevel
                                             aCascadeLevel)
  {
    return mAnimationRule[aCascadeLevel];
  }

  const TimeStamp& AnimationRuleRefreshTime(EffectCompositor::CascadeLevel
                                              aCascadeLevel) const
  {
    return mAnimationRuleRefreshTime[aCascadeLevel];
  }
  void UpdateAnimationRuleRefreshTime(EffectCompositor::CascadeLevel
                                        aCascadeLevel,
                                      const TimeStamp& aRefreshTime)
  {
    mAnimationRuleRefreshTime[aCascadeLevel] = aRefreshTime;
  }

  bool CascadeNeedsUpdate() const { return mCascadeNeedsUpdate; }
  void MarkCascadeNeedsUpdate() { mCascadeNeedsUpdate = true; }
  void MarkCascadeUpdated() { mCascadeNeedsUpdate = false; }

  void UpdateAnimationGeneration(nsPresContext* aPresContext);
  uint64_t GetAnimationGeneration() const { return mAnimationGeneration; }

  static nsIAtom** GetEffectSetPropertyAtoms();

private:
  static nsIAtom* GetEffectSetPropertyAtom(CSSPseudoElementType aPseudoType);

  OwningEffectSet mEffects;

  // These style rules contain the style data for currently animating
  // values.  They only match when styling with animation.  When we
  // style without animation, we need to not use them so that we can
  // detect any new changes; if necessary we restyle immediately
  // afterwards with animation.
  EnumeratedArray<EffectCompositor::CascadeLevel,
                  EffectCompositor::CascadeLevel(
                    EffectCompositor::kCascadeLevelCount),
                  RefPtr<AnimValuesStyleRule>> mAnimationRule;

  // A parallel array to mAnimationRule that records the refresh driver
  // timestamp when the rule was last updated. This is used for certain
  // animations which are updated only periodically (e.g. transform animations
  // running on the compositor that affect the scrollable overflow region).
  EnumeratedArray<EffectCompositor::CascadeLevel,
                  EffectCompositor::CascadeLevel(
                    EffectCompositor::kCascadeLevelCount),
                  TimeStamp> mAnimationRuleRefreshTime;

  // Dirty flag to represent when the mWinsInCascade flag on effects in
  // this set might need to be updated.
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

#ifdef DEBUG
  // Track how many iterators are referencing this effect set when we are
  // destroyed, we can assert that nothing is still pointing to us.
  uint64_t mActiveIterators;

  bool mCalledPropertyDtor;
#endif
};

} // namespace mozilla

#endif // mozilla_EffectSet_h
