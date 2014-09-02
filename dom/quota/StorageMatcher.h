/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_patternmatcher_h__
#define mozilla_dom_quota_patternmatcher_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "ArrayCluster.h"
#include "Utilities.h"

BEGIN_QUOTA_NAMESPACE

template <class ValueType, class BaseType = ArrayCluster<nsIOfflineStorage*> >
class StorageMatcher : public ValueType
{
  typedef StorageMatcher<ValueType, BaseType> SelfType;

  struct Closure
  {
    explicit Closure(SelfType& aSelf)
    : mSelf(aSelf), mPattern(EmptyCString()), mIndexes(nullptr)
    { }

    Closure(SelfType& aSelf, const nsACString& aPattern)
    : mSelf(aSelf), mPattern(aPattern), mIndexes(nullptr)
    { }

    Closure(SelfType& aSelf, const nsTArray<uint32_t>* aIndexes)
    : mSelf(aSelf), mPattern(EmptyCString()), mIndexes(aIndexes)
    { }

    Closure(SelfType& aSelf, const nsACString& aPattern,
            const nsTArray<uint32_t>* aIndexes)
    : mSelf(aSelf), mPattern(aPattern), mIndexes(aIndexes)
    { }

    SelfType& mSelf;
    const nsACString& mPattern;
    const nsTArray<uint32_t>* mIndexes;
  };

public:
  template <class T, class U, class V>
  void
  Find(const nsBaseHashtable<T, U, V>& aHashtable,
       const nsACString& aPattern)
  {
    SelfType::Clear();

    Closure closure(*this, aPattern);
    aHashtable.EnumerateRead(SelfType::MatchPattern, &closure);
  }

  template <class T, class U, class V>
  void
  Find(const nsBaseHashtable<T, U, V>& aHashtable,
       const nsTArray<uint32_t>* aIndexes)
  {
    SelfType::Clear();

    Closure closure(*this, aIndexes);
    aHashtable.EnumerateRead(SelfType::MatchIndexes, &closure);
  }

  template <class T, class U, class V>
  void
  Find(const nsBaseHashtable<T, U, V>& aHashtable,
       uint32_t aIndex)
  {
    nsAutoTArray<uint32_t, 1> indexes;
    indexes.AppendElement(aIndex);

    Find(aHashtable, &indexes);
  }

  template <class T, class U, class V>
  void
  Find(const nsBaseHashtable<T, U, V>& aHashtable,
       const nsACString& aPattern,
       const nsTArray<uint32_t>* aIndexes)
  {
    SelfType::Clear();

    Closure closure(*this, aPattern, aIndexes);
    aHashtable.EnumerateRead(SelfType::MatchPatternAndIndexes, &closure);
  }

  template <class T, class U, class V>
  void
  Find(const nsBaseHashtable<T, U, V>& aHashtable,
       const nsACString& aPattern,
       uint32_t aIndex)
  {
    nsAutoTArray<uint32_t, 1> indexes;
    indexes.AppendElement(aIndex);

    Find(aHashtable, aPattern, &indexes);
  }

  template <class T, class U, class V>
  void
  Find(const nsBaseHashtable<T, U, V>& aHashtable)
  {
    SelfType::Clear();

    Closure closure(*this);
    aHashtable.EnumerateRead(SelfType::MatchAll, &closure);
  }

private:
  static PLDHashOperator
  MatchPattern(const nsACString& aKey,
               BaseType* aValue,
               void* aUserArg)
  {
    MOZ_ASSERT(!aKey.IsEmpty(), "Empty key!");
    MOZ_ASSERT(aValue, "Null pointer!");
    MOZ_ASSERT(aUserArg, "Null pointer!");

    Closure* closure = static_cast<Closure*>(aUserArg);

    if (PatternMatchesOrigin(closure->mPattern, aKey)) {
      aValue->AppendElementsTo(closure->mSelf);
    }

    return PL_DHASH_NEXT;
  }

  static PLDHashOperator
  MatchIndexes(const nsACString& aKey,
               BaseType* aValue,
               void* aUserArg)
  {
    MOZ_ASSERT(!aKey.IsEmpty(), "Empty key!");
    MOZ_ASSERT(aValue, "Null pointer!");
    MOZ_ASSERT(aUserArg, "Null pointer!");

    Closure* closure = static_cast<Closure*>(aUserArg);

    for (uint32_t index = 0; index < closure->mIndexes->Length(); index++) {
      aValue->AppendElementsTo(closure->mIndexes->ElementAt(index),
                               closure->mSelf);
    }

    return PL_DHASH_NEXT;
  }

  static PLDHashOperator
  MatchPatternAndIndexes(const nsACString& aKey,
                         BaseType* aValue,
                         void* aUserArg)
  {
    MOZ_ASSERT(!aKey.IsEmpty(), "Empty key!");
    MOZ_ASSERT(aValue, "Null pointer!");
    MOZ_ASSERT(aUserArg, "Null pointer!");

    Closure* closure = static_cast<Closure*>(aUserArg);

    if (PatternMatchesOrigin(closure->mPattern, aKey)) {
      for (uint32_t index = 0; index < closure->mIndexes->Length(); index++) {
        aValue->AppendElementsTo(closure->mIndexes->ElementAt(index),
                                 closure->mSelf);
      }
    }

    return PL_DHASH_NEXT;
  }

  static PLDHashOperator
  MatchAll(const nsACString& aKey,
           BaseType* aValue,
           void* aUserArg)
  {
    MOZ_ASSERT(!aKey.IsEmpty(), "Empty key!");
    MOZ_ASSERT(aValue, "Null pointer!");
    MOZ_ASSERT(aUserArg, "Null pointer!");

    Closure* closure = static_cast<Closure*>(aUserArg);
    aValue->AppendElementsTo(closure->mSelf);

    return PL_DHASH_NEXT;
  }
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_patternmatcher_h__
