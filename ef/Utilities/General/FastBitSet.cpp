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

#include "FastBitSet.h"
#ifdef WIN32
#include "Value.h" // for leastSigBit
#endif

static PRUint32 nBit[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

static PRUint32 endMasks[32] = 
{
  0x00000001, 0x00000003, 0x00000007, 0x0000000f, 
  0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff, 
  0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff, 
  0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff, 
  0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff, 
  0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff, 
  0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff, 
  0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff
};

static PRUint32 beginMasks[32] = 
{
  0xffffffff, 0xfffffffe, 0xfffffffc, 0xfffffff8,
  0xfffffff0, 0xffffffe0, 0xffffffc0, 0xffffff80,
  0xffffff00, 0xfffffe00, 0xfffffc00, 0xfffff800,
  0xfffff000, 0xffffe000, 0xffffc000, 0xffff8000,
  0xffff0000, 0xfffe0000, 0xfffc0000, 0xfff80000,
  0xfff00000, 0xffe00000, 0xffc00000, 0xff800000,
  0xff000000, 0xfe000000, 0xfc000000, 0xf8000000,
  0xf0000000, 0xe0000000, 0xc0000000, 0x80000000
};


#if defined(WIN32)

PRInt32 FastBitSet::
nextOne(PRInt32 pos) const
{
  ++pos;
  register PRUint32 index = pos >> 5;
  if (index >= nWords) 
	  return -1;
  register PRUint32 offset = pos & 31;
  register PRUint32* ptr = &words[index];
  register PRUint32* limit = &words[nWords];
 
  PRUint32 word = *ptr++ >> offset;
  if (word != 0)
	return leastSigBit(word) + (index << 5) + offset;

  for (++index; ptr < limit; ++index)
	{
	  word = *ptr++;

	  if (word != 0)
		return leastSigBit(word) + (index << 5);
	}
 return -1;
}

#elif 0 

PRInt32 FastBitSet::
nextOne(PRInt32 pos) const
{
  ++pos;
  register PRUint32 index = pos >> 5;
  if (index >= nWords) 
	  return -1;
  register PRUint32 offset = pos & 31;
  register PRUint32* ptr = &words[index];
  register PRUint32* limit = &words[nWords];
 
  PRUint32 word = *ptr++ >> offset;
  if (word != 0)
	{
	  register PRInt32 pos;
	  __asm__("bsf %1,%0\n" : "=r" (pos) : "r" (word));
	  return pos + (index << 5) + offset;
	}

  for (++index; ptr < limit; ++index)
	{
	  word = *ptr++;

	  if (word != 0)
		{
		  register PRInt32 pos;
		  __asm__("bsf %1,%0\n" : "=r" (pos) : "r" (word));
		  return pos + (index << 5);
		}
	}
 return -1;
}

#elif 1

PRInt32 FastBitSet::
nextOne(PRInt32 pos) const
{
  ++pos;
  register PRUint32 index = pos >> 5;
  if (index >= nWords) 
	  return -1;
  register PRUint32 offset = pos & 31;
  register PRUint32* ptr = &words[index];
 
  PRUint32 word = *ptr++ >> offset;
  if (word != 0)
	{
	  while ((word & 1) == 0)
		{
		  offset++; 
		  word >>= 1;
		}
	  return (index << 5) + offset;
	}

  register PRUint32* limit = &words[nWords];

  for (++index; ptr < limit; ++index)
	{
	  word = *ptr++;
	  if (word != 0)
		{
		  offset = 0;
		  while ((word & 1) == 0)
			{
			  offset++; 
			  word >>= 1;
			}
		  return (index << 5) + offset;
		}
	}
 return -1;
}

#else

PRInt32 FastBitSet::
nextOne(PRInt32 pos) const
{
  ++pos;
  register PRUint32 index = pos >> 5;
  if (index >= nWords) 
	  return -1;
  register PRUint32 offset = pos & 31;
  register PRUint32* ptr = &words[index];
  register PRUint32* limit = &words[nWords];
 
  PRUint32 word = *ptr++ >> offset;
  for (; offset < 32 && word != 0; offset++)
	{
	  if (word & 1) 
		return (index << 5) + offset;
	  word >>= 1;
	}
  for (++index; ptr < limit; ++index)
	{
	  word = *ptr++;
	  for (offset = 0; offset < 32 && word != 0; ++offset)
		{
		  if (word & 1)
			return (index << 5) + offset;
		  word >>= 1;
		}
	}
 return -1;
}
#endif

PRInt32 FastBitSet::
nextZero(PRInt32 pos) const
{
  ++pos;
  register PRUint32 index = pos >> 5;
  if (index >= nWords) 
	  return -1;
  register PRUint32 offset = pos & 31;
  register PRUint32* ptr = &words[index];
  register PRUint32* limit = &words[nWords];
 
  PRUint32 word = *ptr++ >> offset;
  for (; offset < 32 && word != PRUint32(~0); offset++)
	{
	  if ((word & 1) == 0)
		return (index << 5) + offset;
	  word >>= 1;
	}
  for (++index; ptr < limit; ++index)
	{
	  word = *ptr++;
	  for (offset = 0; offset < 32 && word != PRUint32(~0); ++offset)
		{
		  if ((word & 1) == 0)
			return (index << 5) + offset;
		  word >>= 1;
		}
	}
  return -1;
}

PRInt32 FastBitSet::
previousOne(PRInt32 pos) const
{
  if (pos == 0)
	return -1;

  --pos;
  register PRUint32 index = pos >> 5;
  if (index >= nWords) 
	  return -1;
  register PRUint32 offset = pos & 31;
  register PRUint32* ptr = &words[index];
 
  PRUint32 word = *ptr-- << (31 - offset);
  if (word != 0)
	{
	  while ((word & 0x80000000) == 0)
		{
		  offset--; 
		  word <<= 1;
		}
	  return (index << 5) + offset;
	}

  register PRUint32* limit = &words[0];

  for (--index; ptr >= limit; --index)
	{
	  word = *ptr--;
	  if (word != 0)
		{
		  offset = 31;
		  while ((word & 0x80000000) == 0)
			{
			  offset--; 
			  word <<= 1;
			}
		  return (index << 5) + offset;
		}
	}
 return -1;
}

PRUint32 FastBitSet::
countOnes()
{
  register PRUint32* ptr = words;
  register int size = nWords-1;
  register PRUint32 counter = 0;

  do
	{
	  register PRUint32 word = ptr[size];
	  if (word != 0)
		{
		  counter += nBit[word & 0xf]; word >>= 4;
		  counter += nBit[word & 0xf]; word >>= 4;
		  counter += nBit[word & 0xf]; word >>= 4;
		  counter += nBit[word & 0xf]; word >>= 4;
		  counter += nBit[word & 0xf]; word >>= 4;
		  counter += nBit[word & 0xf]; word >>= 4;
		  counter += nBit[word & 0xf]; word >>= 4;
		  counter += nBit[word & 0xf];
		}
	}
  while (--size >= 0);
  return counter;
}

PRUint32 FastBitSet::
countOnes(PRUint32 from, PRUint32 to)
{
  register PRUint32* ptr = &words[from >> 5];
  register PRUint32* limit = &words[to >> 5];
  register PRUint32  counter = 0;

  if (ptr == limit)
	{
	  PRUint32 word = (*ptr & endMasks[to & 31]) >> (from & 31);
	  while (word != PRUint32(0))
		{
		  if (word & 1)
			counter++;
		  word >>= 1;
		}
	}
  else
	{
	  PRUint32 word = *ptr++ >> (from & 31);
	  while (word != PRUint32(0))
		{
		  if (word & 1)
			counter++;
		  word >>= 1;
		}
	  while (ptr < limit)
		{
		  word = *ptr++;
		  if (word != 0)
			{
			  counter += nBit[word & 0xf]; word >>= 4;
			  counter += nBit[word & 0xf]; word >>= 4;
			  counter += nBit[word & 0xf]; word >>= 4;
			  counter += nBit[word & 0xf]; word >>= 4;
			  counter += nBit[word & 0xf]; word >>= 4;
			  counter += nBit[word & 0xf]; word >>= 4;
			  counter += nBit[word & 0xf]; word >>= 4;
			  counter += nBit[word & 0xf];
			}
		}
	  word = *ptr & endMasks[to & 31];
	  while (word != PRUint32(0))
		{
		  if (word & 1)
			counter++;
		  word >>= 1;
		}
	}

  return counter;
}

bool FastBitSet::
setAndTest(const FastBitSet& x)
{
  register PRUint32* ptr = words;
  register int size = nWords-1;
  register PRUint32* src = &x.words[size];
  bool ret = false;

  do 
	{
	  register PRUint32 old = ptr[size];
	  register PRUint32 modified = old | *src--;
	  if (old != modified)
		ret = true;
	  ptr[size] = modified;
	}
  while (--size >= 0);
  return ret;
}

void FastBitSet::
set(PRUint32 from, PRUint32 to)
{
  register PRUint32* begin = &words[from >> 5];
  register PRUint32* end = &words[to >> 5];

  PRUint32 beginMask = beginMasks[from & 31];
  PRUint32 endMask = endMasks[to & 31];

  if (begin == end)
	{
	  *begin |= (beginMask & endMask);
	}
  else
	{
	  register PRUint32 val = ~0;
	  *begin++ |= beginMask;
	  while (begin < end)
		*begin++ = val;
	  *end |= endMask;
	}
}

void FastBitSet::
clear(PRUint32 from, PRUint32 to)
{
  register PRUint32* begin = &words[from >> 5];
  register PRUint32* end = &words[to >> 5];

  PRUint32 beginMask = ~beginMasks[from & 31];
  PRUint32 endMask = ~endMasks[to & 31];

  if (begin == end)
	{
	  *begin &= beginMask | endMask;
	}
  else
	{
	  register PRUint32 val = 0;
	  *begin++ &= beginMask;
	  while (begin < end)
		*begin++ = val;
	  *end &= endMask;
	}
}

#ifdef DEBUG_LOG
void FastBitSet::
printPrettyOnes(FILE *f)
{
  fprintf(f, "[ ");  
  for (PRInt32 i = firstOne(); i != -1; i = nextOne(i))
	fprintf(f, "%d ", i);
  fprintf(f, "]\n");
}

void FastBitSet::
printPrettyZeros(FILE *f)
{
  fprintf(f, "[ ");  
  for (PRInt32 i = firstZero(); i != -1; i = nextZero(i))
	fprintf(f, "%d ", i);
  fprintf(f, "]\n");
}

void FastBitSet::
printPretty(FILE *f)
{
  fprintf(f, "[");
  for (PRUint32 i = 0; i < nBits; i++)
	fprintf(f, test(i) ? "1" : "0");
  fprintf(f, "]\n");
}

void FastBitSet::
printDiff(FILE *f, const FastBitSet& x)
{
  fprintf(f, "[");
  for (PRUint32 i = 0; i < nBits; i++)
	if (test(i) != x.test(i))
	  fprintf(f, test(i) ? "-" : "+");
	else
	  fprintf(f, test(i) ? "1" : "0");
  fprintf(f, "]\n");
}
#endif
