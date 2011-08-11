/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Trevor Saunders <trev.saunders@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef RELATION_H_
#define RELATION_H_

#include "AccIterator.h"

/**
 * This class is used to return Relation objects from functions.  A copy
 * constructor doesn't work here because we need to mutate the old relation to
 * have its nsAutoPtr forget what it points to.
 */
struct RelationCopyHelper
{
  RelationCopyHelper(AccIterable* aFirstIter, AccIterable* aLastIter) :
    mFirstIter(aFirstIter), mLastIter(aLastIter) { }

  AccIterable* mFirstIter;
  AccIterable* mLastIter;
};

/**
 * A collection of relation targets of a certain type.  Targets are computed
 * lazily while enumerating.
 */
class Relation
{
public:
  Relation() : mFirstIter(nsnull), mLastIter(nsnull) { }

  Relation(const RelationCopyHelper aRelation) :
    mFirstIter(aRelation.mFirstIter), mLastIter(aRelation.mLastIter) { }

  Relation(AccIterable* aIter) : mFirstIter(aIter), mLastIter(aIter) { }

  Relation(nsAccessible* aAcc) :
    mFirstIter(nsnull), mLastIter(nsnull)
    { AppendTarget(aAcc); }

  Relation(nsIContent* aContent) :
    mFirstIter(nsnull), mLastIter(nsnull)
    { AppendTarget(aContent); }

  Relation& operator = (const RelationCopyHelper& aRH)
  {
    mFirstIter = aRH.mFirstIter;
    mLastIter = aRH.mLastIter;
    return *this;
  }

  Relation& operator = (Relation& aRelation)
  {
    mFirstIter = aRelation.mFirstIter;
    mLastIter = aRelation.mLastIter;
    return *this;
  }

  operator RelationCopyHelper()
  {
    return RelationCopyHelper(mFirstIter.forget(), mLastIter);
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
  inline void AppendTarget(nsAccessible* aAcc)
  {
    if (aAcc)
      AppendIter(new SingleAccIterator(aAcc));
  }

  /**
   * Append the one accessible for this content node to the set of related
   * accessibles.
   */
  inline void AppendTarget(nsIContent* aContent)
  {
    if (aContent)
      AppendTarget(GetAccService()->GetAccessible(aContent));
  }

  /**
   * compute and return the next related accessible.
   */
  inline nsAccessible* Next()
  {
    nsAccessible* target = nsnull;

    // a trick nsAutoPtr deletes what it used to point to when assigned to
    while (mFirstIter && !(target = mFirstIter->Next()))
      mFirstIter = mFirstIter->mNextIter;

    if (!mFirstIter)
      mLastIter = nsnull;

    return target;
  }

private:
  Relation& operator = (const Relation&);

  nsAutoPtr<AccIterable> mFirstIter;
  AccIterable* mLastIter;
};

#endif

