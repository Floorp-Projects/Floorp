/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGATTRTEAROFFTABLE_H_
#define NS_SVGATTRTEAROFFTABLE_H_

#include "nsDataHashtable.h"
#include "nsDebug.h"
#include "nsHashKeys.h"

/**
 * Global hashmap to associate internal SVG data types (e.g. nsSVGLength2) with
 * DOM tear-off objects (e.g. nsIDOMSVGLength). This allows us to always return
 * the same object for subsequent requests for DOM objects.
 *
 * We don't keep an owning reference to the tear-off objects so they are
 * responsible for removing themselves from this table when they die.
 */
template<class SimpleType, class TearoffType>
class nsSVGAttrTearoffTable
{
public:
#ifdef DEBUG
  ~nsSVGAttrTearoffTable()
  {
    NS_ABORT_IF_FALSE(!mTable, "Tear-off objects remain in hashtable at shutdown.");
  }
#endif

  TearoffType* GetTearoff(SimpleType* aSimple);

  void AddTearoff(SimpleType* aSimple, TearoffType* aTearoff);

  void RemoveTearoff(SimpleType* aSimple);

private:
  typedef nsPtrHashKey<SimpleType> SimpleTypePtrKey;
  typedef nsDataHashtable<SimpleTypePtrKey, TearoffType* > TearoffTable;

  TearoffTable* mTable;
};

template<class SimpleType, class TearoffType>
TearoffType*
nsSVGAttrTearoffTable<SimpleType, TearoffType>::GetTearoff(SimpleType* aSimple)
{
  if (!mTable)
    return nullptr;

  TearoffType *tearoff = nullptr;

#ifdef DEBUG
  bool found =
#endif
    mTable->Get(aSimple, &tearoff);
  NS_ABORT_IF_FALSE(!found || tearoff,
      "NULL pointer stored in attribute tear-off map");

  return tearoff;
}

template<class SimpleType, class TearoffType>
void
nsSVGAttrTearoffTable<SimpleType, TearoffType>::AddTearoff(SimpleType* aSimple,
                                                          TearoffType* aTearoff)
{
  if (!mTable) {
    mTable = new TearoffTable;
  }

  // We shouldn't be adding a tear-off if there already is one. If that happens,
  // something is wrong.
  if (mTable->Get(aSimple, nullptr)) {
    NS_ABORT_IF_FALSE(false, "There is already a tear-off for this object.");
    return;
  }

  mTable->Put(aSimple, aTearoff);
}

template<class SimpleType, class TearoffType>
void
nsSVGAttrTearoffTable<SimpleType, TearoffType>::RemoveTearoff(
    SimpleType* aSimple)
{
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

#endif // NS_SVGATTRTEAROFFTABLE_H_
