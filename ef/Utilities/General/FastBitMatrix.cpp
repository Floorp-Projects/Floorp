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

#include "FastBitMatrix.h"
#include "FastBitSet.h"
#include "prlog.h"

bool 
operator == (const FastBitMatrix& x, const FastBitMatrix& y)
{
  if ((x.nCols != y.nCols) || (x.nRows != y.nRows))
	return false;

  PRUint32 nCols = x.nCols;
  PRUint32 nRows = x.nRows;
  for (PRUint32 r = 0; r < nRows; r++)
	{
	  FastBitSet xRow(x.getRow(r), nCols);
	  FastBitSet yRow(y.getRow(r), nCols);
	  
	  if (xRow != yRow)
		return false;
	}
  return true;
}

void FastBitMatrix::
reserve(PRUint32 r, PRUint32 c)
{
  PRUint32 nWordsOnARow = FBS_ROUNDED_WORDS(c);
  nRows = r;
  nCols = c;
  
  if ((nWordsOnARow & (nWordsOnARow - 1)) == 0)
	{
	  PR_FLOOR_LOG2(rowShift, nWordsOnARow);
	}
  else
	{
	  PR_FLOOR_LOG2(rowShift, nWordsOnARow);
	  rowShift++;
	}

  assert(rowShift < 32);

  nWords = r << rowShift;
  if (pool != NULL)
	words = new(*pool) PRUint32[nWords];
  else
	words = new PRUint32[nWords];
}

void FastBitMatrix::
reserveOrUpdate(PRUint32 r, PRUint32 c)
{
  PRUint32 nWordsOnARow = FBS_ROUNDED_WORDS(c);
  PRUint32 neededWords;
  nRows = r;
  nCols = c;
  
  if ((nWordsOnARow & (nWordsOnARow - 1)) == 0)
	{
	  PR_FLOOR_LOG2(rowShift, nWordsOnARow);
	}
  else
	{
	  PR_FLOOR_LOG2(rowShift, nWordsOnARow);
	  rowShift++;
	}
  assert(rowShift < 32);
  neededWords = r << rowShift;
  if (nWords < neededWords)
	{
	  if ((words != NULL) && (pool == NULL)) delete words;
	  nWords = neededWords;
	  if (pool != NULL)
		words = new(*pool) PRUint32[nWords];
	  else
		words = new PRUint32[nWords];
	}
}

void FastBitMatrix::
copy(PRUint32* src)
{
  PRUint32 *ptr = words;
  PRUint32* limit = &words[nRows << rowShift];
  while (ptr < limit) *ptr++ = *src++;
}

#ifdef DEBUG_LOG
void FastBitMatrix::
printPretty(FILE* f)
{
  fprintf(f, "[\n");
  for (PRUint32 i = 0; i < nRows; i++)
	{	
	  fprintf(stdout, " ");
	  FastBitSet row(&words[i << rowShift], nCols);
	  row.printPretty(f);
	}
  fprintf(stdout, "]\n");
}

void FastBitMatrix::
printDiff(FILE *f, const FastBitMatrix& x)
{
  PR_ASSERT((x.nCols == nCols) && (x.nRows == nRows));

  fprintf(f, "[\n");
  for (PRUint32 i = 0; i < nRows; i++)
	{	
	  fprintf(stdout, " ");
	  FastBitSet row(&words[i << rowShift], nCols);
	  FastBitSet xRow(&x.words[i << rowShift], nCols);
	  row.printDiff(f, xRow);
	}
  fprintf(stdout, "]\n");
}
#endif
