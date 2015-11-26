/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EffectSet_h
#define mozilla_EffectSet_h

#include "nsCSSPseudoElements.h" // For nsCSSPseudoElements::Type
#include "nsHashKeys.h" // For nsPtrHashKey
#include "nsTHashtable.h" // For nsTHashtable

namespace mozilla {

namespace dom {
class Element;
class KeyframeEffectReadOnly;
} // namespace dom

// A wrapper around a hashset of AnimationEffect objects to handle
// storing the set as a property of an element.
class EffectSet
{
public:
  EffectSet()
#ifdef DEBUG
    : mCalledPropertyDtor(false)
#endif
  {
    MOZ_COUNT_CTOR(EffectSet);
  }

  ~EffectSet()
  {
    MOZ_ASSERT(mCalledPropertyDtor,
               "must call destructor through element property dtor");
    MOZ_COUNT_DTOR(EffectSet);
  }
  static void PropertyDtor(void* aObject, nsIAtom* aPropertyName,
                           void* aPropertyValue, void* aData);

  static EffectSet* GetEffectSet(dom::Element* aElement,
                                 nsCSSPseudoElements::Type aPseudoType);
  static EffectSet* GetOrCreateEffectSet(dom::Element* aElement,
                                         nsCSSPseudoElements::Type aPseudoType);

  void AddEffect(dom::KeyframeEffectReadOnly& aEffect);
  void RemoveEffect(dom::KeyframeEffectReadOnly& aEffect);

private:
  // We store a raw (non-owning) pointer here because KeyframeEffectReadOnly
  // keeps a strong reference to the target element where this object is
  // stored. KeyframeEffectReadOnly is responsible for removing itself from
  // this set when it releases its reference to the target element.
  typedef nsTHashtable<nsPtrHashKey<dom::KeyframeEffectReadOnly>> EffectPtrSet;

  static nsIAtom* GetEffectSetPropertyAtom(nsCSSPseudoElements::Type
                                             aPseudoType);

  EffectPtrSet mEffects;

#ifdef DEBUG
  bool mCalledPropertyDtor;
#endif
};

} // namespace mozilla

#endif // mozilla_EffectSet_h
