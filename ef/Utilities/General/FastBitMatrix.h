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

#ifndef _FAST_BITMATRIX_H_
#define _FAST_BITMATRIX_H_

#include "Fundamentals.h"
#include "FastBitSet.h"
#include "Pool.h"
#include "prbit.h"

class FastBitMatrix
{
private:
  Pool*     pool;
  PRUint32* words;
  PRUint32  nWords;
  PRUint32  nCols;
  PRUint32  nRows;
  PRUint8   rowShift;


  void reserve(PRUint32 r, PRUint32 c);
  void reserveOrUpdate(PRUint32 r, PRUint32 c);

  void copy(PRUint32* src);

public:
  FastBitMatrix() : pool(NULL), words(0), nWords(0), nCols(0), nRows(0), rowShift(0) {}
  FastBitMatrix(Pool& p) : pool(&p), words(0), nWords(0), nCols(0), nRows(0), rowShift(0) {}
  FastBitMatrix(PRUint32 r, PRUint32 c) : pool(NULL), words(0), nWords(0) {sizeToAndClear(r, c);}
  FastBitMatrix(Pool& p, PRUint32 r, PRUint32 c) : pool(&p), words(0), nWords(0) {sizeToAndClear(r, c);}
  inline FastBitMatrix(const FastBitMatrix& m);
  ~FastBitMatrix() {if ((words != NULL) && (pool == NULL)) delete words;}

  inline FastBitMatrix& operator = (const FastBitMatrix& m);
  friend bool operator == (const FastBitMatrix& x, const FastBitMatrix& y);
  inline friend bool operator != (const FastBitMatrix& x, const FastBitMatrix& y);
  
  inline void sizeTo(PRUint32 r, PRUint32 c) {reserveOrUpdate(r, c);}
  inline void sizeToAndClear(PRUint32 r, PRUint32 c) {sizeTo(r, c); clear();}

  inline void set(PRUint32 r, PRUint32 c);
  inline void setRow(PRUint32 r);
  inline void set();

  inline void clear(PRUint32 r, PRUint32 c);
  inline void clearRow(PRUint32 r);
  inline void clear();

  inline bool test(PRUint32 r, PRUint32 c) const;
  inline bool test(PRUint32 r, PRUint32* bits) const;

  inline PRUint32* getRow(PRUint32 r) {return &words[r << rowShift];}
  inline PRUint32* getRow(PRUint32 r) const {return &words[r << rowShift];}
  inline void orRow(PRUint32 r, FastBitSet& set);
  inline void andRow(PRUint32 r, FastBitSet& set);

  inline void andRows(PRUint32 r, PRUint32 d);
  inline void orRows(PRUint32 r, PRUint32 d);
  inline void copyRows(PRUint32 r, PRUint32 d);
  inline bool compareRows(PRUint32 r, PRUint32 d);
  
#ifdef DEBUG_LOG
  void printPretty(FILE *f);
  void printDiff(FILE *f, const FastBitMatrix& x);
#endif
};

inline bool FastBitMatrix::compareRows(PRUint32 s, PRUint32 d)
{
  register PRUint32* src = &words[s << rowShift];
  register PRUint32* dst = &words[d << rowShift];
  register PRUint32* limit = &src[FBS_ROUNDED_WORDS(nCols)];

  while (src < limit)
	if (*src++ != *dst++)
	  return false;
  return true;
}

inline void FastBitMatrix::copyRows(PRUint32 s, PRUint32 d)
{
  register PRUint32* src = &words[s << rowShift];
  register PRUint32* dst = &words[d << rowShift];
  register PRUint32* limit = &src[FBS_ROUNDED_WORDS(nCols)];

  while (src < limit)
	*dst++ = *src++;
}

inline void FastBitMatrix::andRows(PRUint32 s, PRUint32 d)
{
  register PRUint32* src = &words[s << rowShift];
  register PRUint32* dst = &words[d << rowShift];
  register PRUint32* limit = &src[FBS_ROUNDED_WORDS(nCols)];

  while (src < limit)
	*dst++ &= *src++;
}

inline void FastBitMatrix::orRows(PRUint32 s, PRUint32 d)
{
  register PRUint32* src = &words[s << rowShift];
  register PRUint32* dst = &words[d << rowShift];
  register PRUint32* limit = &src[FBS_ROUNDED_WORDS(nCols)];

  while (src < limit)
	*dst++ |= *src++;
}

inline void FastBitMatrix::orRow(PRUint32 r, FastBitSet& set)
{
  register PRUint32* ptr = &words[r << rowShift];
  register PRUint32* src = set.getWords();
  register int size = FBS_ROUNDED_WORDS(nCols) - 1;
  do ptr[size] |= src[size]; while (--size >= 0);
}

inline void FastBitMatrix::andRow(PRUint32 r, FastBitSet& set)
{
  register PRUint32* ptr = &words[r << rowShift];
  register PRUint32* src = set.getWords();
  register int size = FBS_ROUNDED_WORDS(nCols) - 1;
  do ptr[size] &= src[size]; while (--size >= 0);
}

inline void FastBitMatrix::set(PRUint32 r, PRUint32 c)
{
  register PRUint8 shift = c & 31;
  register PRUint32 mask = (1 << shift);
  words[(r << rowShift) + (c >> 5)] |= mask;
}

inline void FastBitMatrix::setRow(PRUint32 r)
{
  register PRUint32* ptr = &words[r << rowShift];
  register int size = FBS_ROUNDED_WORDS(nCols) - 1;
  do ptr[size] = ~0; while (--size >= 0);
}

inline void FastBitMatrix::set()
{
  register PRUint32* ptr = words;
  register PRUint32 val = ~0;
  register int size = nWords-1;
  do ptr[size] = val; while (--size >= 0);
}

inline void FastBitMatrix::clear(PRUint32 r, PRUint32 c)
{
  register PRUint8 shift = c & 31;
  register PRUint32 mask = (1 << shift);
  mask = ~mask;
  words[(r << rowShift) + (c >> 5)] &= mask;
}

inline void FastBitMatrix::clearRow(PRUint32 r)
{
  register PRUint32* ptr = &words[r << rowShift];
  register int size = FBS_ROUNDED_WORDS(nCols) - 1;
  do ptr[size] = 0; while (--size >= 0);
}

inline void FastBitMatrix::clear()
{
  register PRUint32* ptr = words;
  register PRUint32 val = 0;
  register int size = nWords-1;
  do ptr[size] = val; while (--size >= 0);
}

inline bool FastBitMatrix::test(PRUint32 r, PRUint32 c) const
{
  register PRUint8 shift = c & 31;
  register PRUint32 mask = (1 << shift);
  if ((words[(r << rowShift) + (c >> 5)] & mask) > 0)
	return true;
  else
	return false;
}

inline bool FastBitMatrix::test(PRUint32 r, PRUint32* bits) const
{
  register PRUint32* row = &words[r << rowShift];
  register int size = FBS_ROUNDED_WORDS(nCols) - 1;
  
  do if (row[size] & bits[size]) return true; while (--size >= 0);
  return false;
}

inline FastBitMatrix& FastBitMatrix::
operator = (const FastBitMatrix& m)
{
  reserveOrUpdate(m.nRows, m.nCols);
  copy(m.words);
  return (*this);
}

inline FastBitMatrix::
FastBitMatrix(const FastBitMatrix& m) : pool(m.pool), words(0), nWords(0)
{
  reserve(m.nRows, m.nCols);
  copy(m.words);
}

inline bool
operator != (const FastBitMatrix& x, const FastBitMatrix& y)
{
	return !(x==y);
}

#endif /* _FAST_BITMATRIX_H_ */
