/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _FAST_BITSET_H_
#define _FAST_BITSET_H_

#include "Fundamentals.h"
#include "Pool.h"

#define FBS_ROUNDED_WORDS(x) (((x) + 31) >> 5)

class FastBitSet
{
private:
  Pool*     pool;
  PRUint32* words;
  PRUint32  nWords;
  PRUint32  nBits;
  bool      wordsAreMine;
  
  inline void reserve(PRUint32 n);
  inline void copy(PRUint32* src, PRUint32 size);

public:
  FastBitSet() : pool(NULL), words(0), nWords(0), nBits(0), wordsAreMine(false) {}
  FastBitSet(Pool& p) : pool(&p), words(0), nWords(0), nBits(0), wordsAreMine(false) {}
  FastBitSet(PRUint32 n) : pool(NULL), words(0), nWords(0), nBits(n), wordsAreMine(false) {sizeToAndClear(n);}
  FastBitSet(Pool& p, PRUint32 n) : pool(&p), words(0), nWords(0), nBits(n), wordsAreMine(false) {sizeToAndClear(n);}
  FastBitSet(PRUint32* ptr, PRUint32 n) : pool(NULL), words(ptr), nWords(FBS_ROUNDED_WORDS(n)), nBits(n), wordsAreMine(false) {}
  FastBitSet(const FastBitSet& x) : pool(x.pool), words(0), nWords(x.nWords), nBits(x.nBits), wordsAreMine(false) {sizeTo(x.nBits); copy(x.words, x.nWords);}
  ~FastBitSet() {if (words && wordsAreMine) delete words;}

  inline PRUint32* getWords() {return words;}

  inline void copyFrom(const FastBitSet& x) {copy(x.words, x.nWords);}
  inline void operator = (const FastBitSet& x) {sizeTo(x.nBits); copy(x.words, x.nWords);}
  inline FastBitSet& operator |= (const FastBitSet& x);
  inline FastBitSet& operator &= (const FastBitSet& x);
  inline FastBitSet& operator -= (const FastBitSet& x);
  inline friend bool operator == (const FastBitSet& x, const FastBitSet& y);
  inline friend bool operator != (const FastBitSet& x, const FastBitSet& y);

  inline void sizeTo(PRUint32 n) {PR_ASSERT(n); nBits = n; if (nWords < FBS_ROUNDED_WORDS(n)) reserve(n);}
  inline void sizeTo(Pool& p, PRUint32 n) {PR_ASSERT(n); pool = &p; nBits = n; if (nWords < FBS_ROUNDED_WORDS(n)) reserve(n);}
  inline void sizeToAndClear(PRUint32 n) {PR_ASSERT(n); sizeTo(n); clear();}
  inline void sizeToAndClear(Pool& p, PRUint32 n) {PR_ASSERT(n); sizeTo(p, n); clear();}

  inline bool empty();

  inline void clear();
  inline void clear(PRUint32 n);
  void clear(PRUint32 from, PRUint32 to);

  inline void set();
  inline void set(PRUint32 n);
  void set(PRUint32 from, PRUint32 to);

  bool setAndTest(const FastBitSet& x);

  inline bool test(PRUint32 n) const;

  inline PRInt32 firstOne() const {return nextOne(-1);}
  PRInt32 nextOne(PRInt32 pos) const;
  inline PRInt32 lastOne() const {return previousOne(nBits);}
  PRInt32 previousOne(PRInt32 pos) const;

  inline PRInt32 firstZero() const {return nextZero(-1);}
  PRInt32 nextZero(PRInt32 pos) const;

  PRUint32 countOnes();
  PRUint32 countOnes(PRUint32 from, PRUint32 to);
  inline PRUint32 countZeros() {return nBits - countOnes();}

#ifdef DEBUG_LOG
  void printPretty(FILE *f);
  void printDiff(FILE *f, const FastBitSet& x);
  void printPrettyOnes(FILE *f);
  void printPrettyZeros(FILE *f);
#endif
};

inline bool 
operator == (const FastBitSet& x, const FastBitSet& y)
{
  register PRUint32* xWords = x.words;
  register PRUint32* yWords = y.words;
  register int size = x.nWords-1;

  do if (xWords[size] != yWords[size]) return false; while (--size >= 0);
  return true;
}

inline bool
operator != (const FastBitSet& x, const FastBitSet& y)
{
	return !(x==y);
}

inline FastBitSet& FastBitSet::operator |= (const FastBitSet& x)
{
  register PRUint32* xWords = words;
  register PRUint32* yWords = x.words;
  register int size = nWords - 1;
  do xWords[size] |= yWords[size]; while (--size >= 0);
  return *this;
}

inline FastBitSet& FastBitSet::operator &= (const FastBitSet& x)
{
  register PRUint32* xWords = words;
  register PRUint32* yWords = x.words;
  register int size = nWords - 1;
  do xWords[size] &= yWords[size]; while (--size >= 0);
  return *this;
}

inline FastBitSet& FastBitSet::operator -= (const FastBitSet& x)
{
  register PRUint32* xWords = words;
  register PRUint32* yWords = x.words;
  register int size = nWords - 1;
  do xWords[size] &= ~yWords[size]; while (--size >= 0);
  return *this;
}

inline bool FastBitSet::test(PRUint32 n) const 
{
  register PRUint8 shift = n & 31;
  register PRUint32 mask = (1 << shift);
  if ((words[n >> 5] & mask) > 0)
	return true;
  else
	return false;
}

inline void FastBitSet::set(PRUint32 n)
{
  register PRUint8 shift = n & 31;
  register PRUint32 mask = (1 << shift);
  words[n >> 5] |= mask;
}

inline void FastBitSet::clear(PRUint32 n)
{
  register PRUint8 shift = n & 31;
  register PRUint32 mask = (1 << shift);
  mask = ~mask;
  words[n >> 5] &= mask;
}

inline bool FastBitSet::empty()
{
  register PRUint32* ptr = words;
  register int size = nWords-1;
  do 
	if (ptr[size] != 0)
	  return false;
  while (--size >= 0);

  return true;
}

inline void FastBitSet::set()
{
  register PRUint32* ptr = words;
  register PRUint32 val = ~0;
  register int size = nWords-1;
  do ptr[size] = val; while (--size >= 0);
}

inline void FastBitSet::clear()
{
  register PRUint32* ptr = words;
  register PRUint32 val = 0;
  register int size = nWords-1;
  do ptr[size] = val; while (--size >= 0);
}

inline void FastBitSet::reserve(PRUint32 n)
{
  if (words && wordsAreMine) delete words;
  nWords = FBS_ROUNDED_WORDS(n);
  if (pool != NULL)
	{
	  words = new(*pool) PRUint32[nWords];
	  wordsAreMine = false;
	}
  else
	{
	  words = new PRUint32[nWords];
	  wordsAreMine = true;
	}
}

inline void FastBitSet::
copy(PRUint32* src, PRUint32 size)
{
  PR_ASSERT(size <= nWords);
  register PRUint32* ptr = words;
  register int len = size-1;
  src = &src[len];
  do ptr[len] = *src--; while (--len >= 0);
  if (size < nWords)
	do ptr[size] = 0; while (++size < nWords);
}


#endif /* _FAST_BITSET_H_ */
