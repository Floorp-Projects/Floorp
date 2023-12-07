/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SMIL_SMILCOMPOSITOR_H_
#define DOM_SMIL_SMILCOMPOSITOR_H_

#include <utility>

#include "PLDHashTable.h"
#include "SMILTargetIdentifier.h"
#include "mozilla/SMILAnimationFunction.h"
#include "mozilla/SMILCompositorTable.h"
#include "mozilla/UniquePtr.h"
#include "nsCSSPropertyID.h"
#include "nsString.h"
#include "nsTHashtable.h"

namespace mozilla {

class ComputedStyle;

//----------------------------------------------------------------------
// SMILCompositor
//
// Performs the composition of the animation sandwich by combining the results
// of a series animation functions according to the rules of SMIL composition
// including prioritising animations.

class SMILCompositor : public PLDHashEntryHdr {
 public:
  using KeyType = SMILTargetIdentifier;
  using KeyTypeRef = const KeyType&;
  using KeyTypePointer = const KeyType*;

  explicit SMILCompositor(KeyTypePointer aKey)
      : mKey(*aKey), mForceCompositing(false) {}
  SMILCompositor(SMILCompositor&& toMove) noexcept
      : PLDHashEntryHdr(std::move(toMove)),
        mKey(std::move(toMove.mKey)),
        mAnimationFunctions(std::move(toMove.mAnimationFunctions)),
        mForceCompositing(false) {}

  // PLDHashEntryHdr methods
  KeyTypeRef GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const;
  static KeyTypePointer KeyToPointer(KeyTypeRef aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey);
  enum { ALLOW_MEMMOVE = false };

  // Adds the given animation function to this Compositor's list of functions
  void AddAnimationFunction(SMILAnimationFunction* aFunc);

  // Composes the attribute's current value with the list of animation
  // functions, and assigns the resulting value to this compositor's target
  // attribute. If a change is made that might produce style updates,
  // aMightHavePendingStyleUpdates is set to true. Otherwise it is not modified.
  void ComposeAttribute(bool& aMightHavePendingStyleUpdates);

  // Clears animation effects on my target attribute
  void ClearAnimationEffects();

  // Cycle-collection support
  void Traverse(nsCycleCollectionTraversalCallback* aCallback);

  // Toggles a bit that will force us to composite (bypassing early-return
  // optimizations) when we hit ComposeAttribute.
  void ToggleForceCompositing() { mForceCompositing = true; }

  // Transfers |aOther|'s mCachedBaseValue to |this|
  void StealCachedBaseValue(SMILCompositor* aOther) {
    mCachedBaseValue = std::move(aOther->mCachedBaseValue);
  }

  bool HasSameNumberOfAnimationFunctionsAs(const SMILCompositor& aOther) const {
    return mAnimationFunctions.Length() == aOther.mAnimationFunctions.Length();
  }

 private:
  // Create a SMILAttr for my target, on the heap.
  //
  // @param aBaseComputedStyle  An optional ComputedStyle which, if set, will be
  //                           used when fetching the base style.
  UniquePtr<SMILAttr> CreateSMILAttr(const ComputedStyle* aBaseComputedStyle);

  // Returns the CSS property this compositor should animate, or
  // eCSSProperty_UNKNOWN if this compositor does not animate a CSS property.
  nsCSSPropertyID GetCSSPropertyToAnimate() const;

  // Returns true if we might need to refer to base styles (i.e. we are
  // targeting a CSS property and have one or more animation functions that
  // don't just replace the underlying value).
  //
  // This might return true in some cases where we don't actually need the base
  // style since it doesn't build up the animation sandwich to check if the
  // functions that appear to need the base style are actually replaced by
  // a function further up the stack.
  bool MightNeedBaseStyle() const;

  // Finds the index of the first function that will affect our animation
  // sandwich. Also toggles the 'mForceCompositing' flag if it finds that any
  // (used) functions have changed.
  uint32_t GetFirstFuncToAffectSandwich();

  // If the passed-in base value differs from our cached base value, this
  // method updates the cached value (and toggles the 'mForceCompositing' flag)
  void UpdateCachedBaseValue(const SMILValue& aBaseValue);

  // The hash key (tuple of element and attributeName)
  KeyType mKey;

  // Hash Value: List of animation functions that animate the specified attr
  nsTArray<SMILAnimationFunction*> mAnimationFunctions;

  // Member data for detecting when we need to force-recompose
  // ---------------------------------------------------------
  // Flag for tracking whether we need to compose. Initialized to false, but
  // gets flipped to true if we detect that something has changed.
  bool mForceCompositing;

  // Cached base value, so we can detect & force-recompose when it changes
  // from one sample to the next. (SMILAnimationController moves this
  // forward from the previous sample's compositor by calling
  // StealCachedBaseValue.)
  SMILValue mCachedBaseValue;
};

}  // namespace mozilla

#endif  // DOM_SMIL_SMILCOMPOSITOR_H_
