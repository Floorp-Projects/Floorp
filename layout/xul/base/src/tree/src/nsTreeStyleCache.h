/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeStyleCache_h__
#define nsTreeStyleCache_h__

#include "nsHashtable.h"
#include "nsIAtom.h"
#include "nsICSSPseudoComparator.h"
#include "nsStyleContext.h"

class nsISupportsArray;

class nsDFAState : public nsHashKey
{
public:
  PRUint32 mStateID;

  nsDFAState(PRUint32 aID) :mStateID(aID) {}

  PRUint32 GetStateID() { return mStateID; }

  PRUint32 HashCode(void) const {
    return mStateID;
  }

  bool Equals(const nsHashKey *aKey) const {
    nsDFAState* key = (nsDFAState*)aKey;
    return key->mStateID == mStateID;
  }

  nsHashKey *Clone(void) const {
    return new nsDFAState(mStateID);
  }
};

class nsTransitionKey : public nsHashKey
{
public:
  PRUint32 mState;
  nsCOMPtr<nsIAtom> mInputSymbol;

  nsTransitionKey(PRUint32 aState, nsIAtom* aSymbol) :mState(aState), mInputSymbol(aSymbol) {}

  PRUint32 HashCode(void) const {
    // Make a 32-bit integer that combines the low-order 16 bits of the state and the input symbol.
    PRInt32 hb = mState << 16;
    PRInt32 lb = (NS_PTR_TO_INT32(mInputSymbol.get()) << 16) >> 16;
    return hb+lb;
  }

  bool Equals(const nsHashKey *aKey) const {
    nsTransitionKey* key = (nsTransitionKey*)aKey;
    return key->mState == mState && key->mInputSymbol == mInputSymbol;
  }

  nsHashKey *Clone(void) const {
    return new nsTransitionKey(mState, mInputSymbol);
  }
};

class nsTreeStyleCache 
{
public:
  nsTreeStyleCache() :mTransitionTable(nsnull), mCache(nsnull), mNextState(0) {}
  ~nsTreeStyleCache() { Clear(); }

  void Clear() { delete mTransitionTable; mTransitionTable = nsnull; delete mCache; mCache = nsnull; mNextState = 0; }

  nsStyleContext* GetStyleContext(nsICSSPseudoComparator* aComparator,
                                  nsPresContext* aPresContext, 
                                  nsIContent* aContent, 
                                  nsStyleContext* aContext,
                                  nsIAtom* aPseudoElement,
                                  nsISupportsArray* aInputWord);

  static bool DeleteDFAState(nsHashKey *aKey, void *aData, void *closure);

  static bool ReleaseStyleContext(nsHashKey *aKey, void *aData, void *closure);

protected:
  // A transition table for a deterministic finite automaton.  The DFA
  // takes as its input a single pseudoelement and an ordered set of properties.  
  // It transitions on an input word that is the concatenation of the pseudoelement supplied
  // with the properties in the array.
  // 
  // It transitions from state to state by looking up entries in the transition table (which is
  // a mapping from (S,i)->S', where S is the current state, i is the next
  // property in the input word, and S' is the state to transition to.
  //
  // If S' is not found, it is constructed and entered into the hashtable
  // under the key (S,i).
  //
  // Once the entire word has been consumed, the final state is used
  // to reference the cache table to locate the style context.
  nsObjectHashtable* mTransitionTable;

  // The cache of all active style contexts.  This is a hash from 
  // a final state in the DFA, Sf, to the resultant style context.
  nsObjectHashtable* mCache;

  // An integer counter that is used when we need to make new states in the
  // DFA.
  PRUint32 mNextState;
};

#endif // nsTreeStyleCache_h__
