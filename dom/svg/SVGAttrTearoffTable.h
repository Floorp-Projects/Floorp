/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGATTRTEAROFFTABLE_H_
#define NS_SVGATTRTEAROFFTABLE_H_

#include "nsDataHashtable.h"
#include "nsDebug.h"
#include "nsHashKeys.h"

namespace mozilla {

/**
 * Global hashmap to associate internal SVG data types (e.g. SVGAnimatedLength)
 * with DOM tear-off objects (e.g. DOMSVGLength). This allows us to always
 * return the same object for subsequent requests for DOM objects.
 *
 * We don't keep an owning reference to the tear-off objects so they are
 * responsible for removing themselves from this table when they die.
 */
template <class SimpleType, class TearoffType>
class SVGAttrTearoffTable {
 public:
#ifdef DEBUG
  ~SVGAttrTearoffTable() {
    MOZ_ASSERT(!mTable, "Tear-off objects remain in hashtable at shutdown.");
  }
#endif

  TearoffType* GetTearoff(SimpleType* aSimple);

  void AddTearoff(SimpleType* aSimple, TearoffType* aTearoff);

  void RemoveTearoff(SimpleType* aSimple);

 private:
  typedef nsPtrHashKey<SimpleType> SimpleTypePtrKey;
  typedef nsDataHashtable<SimpleTypePtrKey, TearoffType*> TearoffTable;

  TearoffTable* mTable;
};

template <class SimpleType, class TearoffType>
TearoffType* SVGAttrTearoffTable<SimpleType, TearoffType>::GetTearoff(
    SimpleType* aSimple) {
  if (!mTable) return nullptr;

  TearoffType* tearoff = nullptr;

#ifdef DEBUG
  bool found =
#endif
      mTable->Get(aSimple, &tearoff);
  MOZ_ASSERT(!found || tearoff,
             "null pointer stored in attribute tear-off map");

  return tearoff;
}

template <class SimpleType, class TearoffType>
void SVGAttrTearoffTable<SimpleType, TearoffType>::AddTearoff(
    SimpleType* aSimple, TearoffType* aTearoff) {
  if (!mTable) {
    mTable = new TearoffTable;
  }

  // We shouldn't be adding a tear-off if there already is one. If that happens,
  // something is wrong.
  if (mTable->Get(aSimple, nullptr)) {
    MOZ_ASSERT(false, "There is already a tear-off for this object.");
    return;
  }

  mTable->Put(aSimple, aTearoff);
}

template <class SimpleType, class TearoffType>
void SVGAttrTearoffTable<SimpleType, TearoffType>::RemoveTearoff(
    SimpleType* aSimple) {
  if (!mTable) {
    // Perhaps something happened in between creating the SimpleType object and
    // registering it
    return;
  }

  mTable->Remove(aSimple);
  if (mTable->Count() == 0) {
    delete mTable;
    mTable = nullptr;
  }
}

}  // namespace mozilla

#endif  // NS_SVGATTRTEAROFFTABLE_H_
