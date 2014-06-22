/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_relation_h_
#define mozilla_a11y_relation_h_

#include "AccIterator.h"

#include "mozilla/Move.h"

namespace mozilla {
namespace a11y {

/**
 * A collection of relation targets of a certain type.  Targets are computed
 * lazily while enumerating.
 */
class Relation
{
public:
  Relation() : mFirstIter(nullptr), mLastIter(nullptr) { }

  Relation(AccIterable* aIter) :
    mFirstIter(aIter), mLastIter(aIter) { }

  Relation(Accessible* aAcc) :
    mFirstIter(nullptr), mLastIter(nullptr)
    { AppendTarget(aAcc); }

  Relation(DocAccessible* aDocument, nsIContent* aContent) :
    mFirstIter(nullptr), mLastIter(nullptr)
    { AppendTarget(aDocument, aContent); }

  Relation(Relation&& aOther) :
    mFirstIter(Move(aOther.mFirstIter)), mLastIter(aOther.mLastIter)
  {
    aOther.mLastIter = nullptr;
  }

  Relation& operator = (Relation&& aRH)
  {
    mFirstIter = Move(aRH.mFirstIter);
    mLastIter = aRH.mLastIter;
    aRH.mLastIter = nullptr;
    return *this;
  }

  inline void AppendIter(AccIterable* aIter)
  {
    if (mLastIter)
      mLastIter->mNextIter = aIter;
    else
      mFirstIter = aIter;

    mLastIter = aIter;
  }

  /**
   * Append the given accessible to the set of related accessibles.
   */
  inline void AppendTarget(Accessible* aAcc)
  {
    if (aAcc)
      AppendIter(new SingleAccIterator(aAcc));
  }

  /**
   * Append the one accessible for this content node to the set of related
   * accessibles.
   */
  void AppendTarget(DocAccessible* aDocument, nsIContent* aContent)
  {
    if (aContent)
      AppendTarget(aDocument->GetAccessible(aContent));
  }

  /**
   * compute and return the next related accessible.
   */
  inline Accessible* Next()
  {
    Accessible* target = nullptr;

    // a trick nsAutoPtr deletes what it used to point to when assigned to
    while (mFirstIter && !(target = mFirstIter->Next()))
      mFirstIter = mFirstIter->mNextIter;

    if (!mFirstIter)
      mLastIter = nullptr;

    return target;
  }

private:
  Relation& operator = (const Relation&) MOZ_DELETE;
  Relation(const Relation&) MOZ_DELETE;

  nsAutoPtr<AccIterable> mFirstIter;
  AccIterable* mLastIter;
};

} // namespace a11y
} // namespace mozilla

#endif

