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

#include "Fundamentals.h"
#include "FastBitSet.h"
#include "Pool.h"

//
// Resize the bitset to sizeInBits. All the bits present in the bitset will be lost.
// The bitset will be unitialized. A call to clear() or set() is needed after sizeTo() !!!
//
void FastBitSet::sizeTo(Pool& pool, Uint32 sizeInBits)
{
	PR_ASSERT(sizeInBits);

	Uint32 newSizeInWords = getNumberOfWords(sizeInBits);
	if (newSizeInWords > sizeInWords) {
		sizeInWords = newSizeInWords;
		bits = new(pool) Uint32[newSizeInWords];
	}
#ifdef DEBUG_LOG
	this->sizeInBits = sizeInBits;
#endif /* DEBUG_LOG */
}


//
// Set the bits (in this bitset) that are defined in x and return true if this bitset changed.
//
bool FastBitSet::set(const FastBitSet& x)
{
	PR_ASSERT(sizeInWords == x.sizeInWords);

	bool differ = false;
	Uint32* dst = bits;
	Uint32* src = x.bits;
	Int32 count = sizeInWords - 1;

	do {
		Uint32 oldWord = dst[count];
		Uint32 newWord = oldWord | src[count];
		differ |= (newWord != oldWord);
		dst[count] = newWord;
	} while(--count >= 0);

	return differ;
}

//
// Arithmetic operators.
//

#define FastBitSetOperation(x, op)														\
	PR_ASSERT(sizeInWords && sizeInWords == x.sizeInWords);								\
	Uint32* dst = bits;																	\
	Uint32* src = x.bits;																\
	Int32 count = sizeInWords < x.sizeInWords ? sizeInWords - 1 : x.sizeInWords - 1;	\
	do {																				\
		dst[count] op src[count];														\
	} while(--count >= 0);


FastBitSet& FastBitSet::operator |= (const FastBitSet& x)
{
	FastBitSetOperation(x, |=);
	return *this;
}

FastBitSet& FastBitSet::operator &= (const FastBitSet& x)
{
	FastBitSetOperation(x, &=);
	return *this;
}
 
FastBitSet& FastBitSet::operator ^= (const FastBitSet& x)
{
	FastBitSetOperation(x, ^=);
	return *this;
}

FastBitSet& FastBitSet::operator -= (const FastBitSet& x)
{
	FastBitSetOperation(x, &= ~);
	return *this;
}

//
// Copy operator.
//
FastBitSet& FastBitSet::operator = (const FastBitSet& x)
{
	FastBitSetOperation(x, = );
	return *this;
}

#undef FastBitSetOperation

//
// Comparison operator.
//
bool operator == (const FastBitSet& x, const FastBitSet& y)
{
	PR_ASSERT(x.sizeInWords == y.sizeInWords);

	Uint32* srcX = x.bits;
	Uint32* srcY = y.bits;
	Uint32* limitX = &srcX[x.sizeInWords];

	while (srcX < limitX)
		if (*srcX++ != *srcY++)
			return false;

	return true;
}

//
// Return the next '1' bit just after pos or -1 if not found.
//
Int32 FastBitSet::nextOne(Int32 pos) const
{
	PR_ASSERT(sizeInWords);

	++pos; // Skip this bit.

	Uint32 wordOffset = getWordOffset(pos);
	if (wordOffset < sizeInWords) {
		Uint32* ptr = &bits[wordOffset];
		Uint8 bitOffset = getBitOffset(pos);
		Uint32 word = *ptr++ >> bitOffset;
		if (word != 0) {
			while ((word & 3) == 0) {
				bitOffset+=2;
				word >>= 2;
			}
			if ((word & 1) == 0)
				bitOffset++;

			return (wordOffset << 5) + bitOffset;
		}

		Uint32* limit = &bits[sizeInWords];
		for (++wordOffset; ptr < limit; ++wordOffset) {
			word = *ptr++;
			if (word != 0) {
				bitOffset = 0;
				while ((word & 3) == 0) {
					bitOffset+=2;
					word >>= 2;
				}
				if ((word & 1) == 0)
					bitOffset++;
			  
				return (wordOffset << 5) + bitOffset;
			}
		}
	}
	return -1;
}

//
// Return the '1' bit just before pos or -1 if not found.
//
Int32 FastBitSet::previousOne(Int32 pos) const
{
	PR_ASSERT(sizeInWords);

	--pos; // Skip this bit.

	Uint32 wordOffset = getWordOffset(pos);
	if (wordOffset < sizeInWords) {
		Uint32* ptr = &bits[wordOffset];
		Uint8 bitOffset = getBitOffset(pos);
		Uint32 word = *ptr-- << (31 - bitOffset);
		if (word != 0) {
			while ((word & 0x80000000) == 0) {
				bitOffset--;
				word <<= 1;
			}
			return (wordOffset << 5) + bitOffset;
		}

		Uint32* limit = bits;
		for (--wordOffset; ptr >= limit; --wordOffset) {
			word = *ptr--;
			if (word != 0) {
				bitOffset = 31;
				while ((word & 0x80000000) == 0) {
					bitOffset--;
					word <<= 1;
				}
			  
				return (wordOffset << 5) + bitOffset;
			}
		}
	}
	return -1;
}

//
// Return the next '0' bit just after pos or -1 if not found.
//
Int32 FastBitSet::nextZero(Int32 pos) const
{
	PR_ASSERT(sizeInWords);

	++pos; // Skip this bit.

	Uint32 wordOffset = getWordOffset(pos);
	if (wordOffset < sizeInWords) {
		Uint32* ptr = &bits[wordOffset];
		Uint8 bitOffset = getBitOffset(pos);
		Uint32 word = *ptr++ >> bitOffset;
		if (word != Uint32(~0)) {
			while (word & 1) {
				bitOffset++;
				word >>= 1;
			}
			return (wordOffset << 5) + bitOffset;
		}

		Uint32* limit = &bits[sizeInWords];
		for (++wordOffset; ptr < limit; ++wordOffset) {
			word = *ptr++;
			if (word != Uint32(~0)) {
				bitOffset = 0;
				while (word & 1) {
					bitOffset++;
					word >>= 1;
				}
				return (wordOffset << 5) + bitOffset;
			}
		}
	}
	return -1;
}

//
// Return the '0' bit just before pos or -1 if not found.
//
Int32 FastBitSet::previousZero(Int32 pos) const
{
	PR_ASSERT(sizeInWords);

	--pos; // Skip this bit.

	Uint32 wordOffset = getWordOffset(pos);
	if (wordOffset < sizeInWords) {
		Uint32* ptr = &bits[wordOffset];
		Uint8 bitOffset = getBitOffset(pos);
		Uint8 shift = (31 - bitOffset);
		Uint32 word = *ptr-- << shift;
		if (word != (Uint32(~0) << shift)) {
			while (word & 0x80000000) {
				bitOffset--;
				word <<= 1;
			}
			return (wordOffset << 5) + bitOffset;
		}

		Uint32* limit = bits;
		for (--wordOffset; ptr >= limit; --wordOffset) {
			word = *ptr--;
			if (word != Uint32(~0)) {
				bitOffset = 31;
				while (word & 0x80000000) {
					bitOffset--;
					word <<= 1;
				}
			  
				return (wordOffset << 5) + bitOffset;
			}
		}
	}
	return -1;
}

//
// Set the bits from position 'from' to position 'to'.
//
void FastBitSet::set(Uint32 from, Uint32 to)
{
	Uint32* ptr = &bits[getWordOffset(from)];
	Uint32* limit = &bits[getWordOffset(to)];
	Uint32 beginMask = ~((1 << getBitOffset(from)) - 1); // 11..(32 - from) times..1100..from times..00
	Uint32 endMask = (2 << getBitOffset(to)) - 1; // 00..(32 - to) times..0011..to times..11

	if (ptr == limit) {
		*ptr |= (beginMask & endMask);
	} else {
		register Uint32 val = ~0;
		*ptr++ |= beginMask;
		while (ptr < limit)
			*ptr++ = val;
		*limit |= endMask;
	}
}

#ifdef DEBUG_LOG
//
// Print the bitset.
//
void FastBitSet::printPretty(FILE* f) const
{
	PR_ASSERT(sizeInWords);

	fprintf(f, "[");
	for (PRUint32 bit = 0; bit < sizeInBits; bit++)
		fprintf(f, test(bit) ? "1" : "0");
	fprintf(f, "]\n");
}

//
// Print the difference between this bitset and the given biset.
//
void FastBitSet::printDiff(FILE *f, const FastBitSet& x) const
{
	fprintf(f, "[");
	for (Uint32 i = 0; i < sizeInBits; i++)
		if (test(i) != x.test(i))
			fprintf(f, test(i) ? "-" : "+");
		else
			fprintf(f, test(i) ? "1" : "0");
	fprintf(f, "]\n");
}

//
// Print all the '1' bits.
//
void FastBitSet::printPrettyOnes(FILE* f) const
{
	PR_ASSERT(sizeInWords);

	fprintf(f, "[ ");
	for (Int32 bit = firstOne(); !done(bit); bit = nextOne(bit))
		fprintf(f, "%d ", bit);
	fprintf(f, "]\n");
}

//
// Print all the '0' bits.
//
void FastBitSet::printPrettyZeros(FILE* f) const
{
	PR_ASSERT(sizeInWords);

	fprintf(f, "[ ");
	for (Int32 bit = firstZero(); !done(bit); bit = nextZero(bit))
		fprintf(f, "%d ", bit);
	fprintf(f, "]\n");
}
#endif /* DEBUG_LOG */
