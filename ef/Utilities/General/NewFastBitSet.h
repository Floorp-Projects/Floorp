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

// ----------------------------------------------------------------------------
// FastBitSet
// 
// !!!! A FastBitSet cannot have a null number bits.
// 

class FastBitSet
{
	friend class FastBitMatrix;			// The bit matrix will need to access bits & sizeInWord.
private:

	Uint32* bits;						// Array of words to store the bits.
	Uint32  sizeInWords;				// Number of words allocated. 
#ifdef DEBUG_LOG
	Uint32  sizeInBits;					// Number of bits (only used in printPretty*()).
#endif /* DEBUG_LOG */

	Uint32 getNumberOfWords(Uint32 numberOfBits) const {return (numberOfBits + 31) >> 5;}
	Uint32 getWordOffset(Uint32 bitPos) const {return bitPos >> 5;}
	Uint8  getBitOffset(Uint32 bitPos) const {return bitPos & 31;}
  
	FastBitSet(const FastBitSet&);		// No Copy constructor.

public:
	// This is not a valid bitset. The method sizeTo must be called before any
	// use of this instance.
	FastBitSet() : sizeInWords(0) {}

	// Instanciate and allocate the bits.
	inline FastBitSet(Pool& pool, Uint32 sizeInBits);
	// This FastBitSet gets its bits from memory.
	inline FastBitSet(Uint32* bits, Uint32 sizeInBits);
	// This FastBitSet copy its bits from memory..
	inline FastBitSet(Pool& pool, Uint32* bits, Uint32 sizeInBits);

	// Resize the bitset to sizeInBits.
	void sizeTo(Pool& pool, Uint32 sizeInBits);
	// Resize the bitset and clear all the bits.
	inline void sizeToAndClear(Pool& pool, Uint32 sizeInBits);

	// Set all the bits in this bitset.
	inline void set();
	// Set the bit at pos.
	inline void set(Uint32 pos) {PR_ASSERT(sizeInWords); bits[getWordOffset(pos)] |= (1<<getBitOffset(pos));}
	// Set the bits from position 'from' to position 'to'.
	void set(Uint32 from, Uint32 to);
	// Set the bits (in this bitset) that are defined in x and return true if this bitset changed.
	bool set(const FastBitSet& x);
	// Clear all the bits in this bitset.
	inline void clear();
	// Clear the bit at pos.
	inline void clear(Uint32 pos) {PR_ASSERT(sizeInWords); bits[getWordOffset(pos)] &= ~(1<<getBitOffset(pos));}
	// Clear the bits from position 'from' to position 'to'.
	inline void clear(Uint32 from, Uint32 to);
	// Clear the bits (in this bitset) that are defined in x and return true if this bitset changed.
	bool clear(const FastBitSet& x);
	// Test the bit at pos.
	inline bool test(Uint32 pos) const {PR_ASSERT(sizeInWords); return (bits[getWordOffset(pos)]>>getBitOffset(pos))&1;}
	// Count the number of bits at '1'.
	inline Uint32 countOnes();
	// Count the number of bits at '0'.
	inline Uint32 countZeros();

	// Return the '1' bit just after pos or -1 if not found.
	Int32 nextOne(Int32 pos) const;
	// Return the '1' bit just before pos or -1 if not found.
	Int32 previousOne(Int32 pos) const;
	// Return the first '1' bit in the bitset or -1 if not found.
	inline Int32 firstOne() const {return nextOne(-1);}
	// Return the last '1' bit in the bitset or -1 if not found.
	inline Int32 lastOne() const {return previousOne(sizeInWords << 5);}
	// Return the '0' bit just after pos or -1 if not found.
	Int32 nextZero(Int32 pos) const;
	// Return the '0' bit just before pos or -1 if not found.
	Int32 previousZero(Int32 pos) const;
	// Return the first '0' bit in the bitset or -1 if not found.
	inline Int32 firstZero() const {return nextZero(-1);}
	// Return the last '1' bit in the bitset or -1 if not found.
	inline Int32 lastZero() const {return previousZero(sizeInWords << 5);}
	// Return true if at the end of the BitSet.
	inline bool done(Int32 pos) const {return pos == -1;}


	// Return true if the bitset is empty.
	inline bool empty() {return done(firstOne());}

	// Arithmetic operators.
	FastBitSet& operator |= (const FastBitSet& x);
	FastBitSet& operator &= (const FastBitSet& x);
	FastBitSet& operator ^= (const FastBitSet& x);
	FastBitSet& operator -= (const FastBitSet& x);

	// Comparison operators.
	friend bool operator == (const FastBitSet& x, const FastBitSet& y);
	friend bool operator != (const FastBitSet& x, const FastBitSet& y) {return !(x == y);}

	// Copy operator.
	FastBitSet& operator = (const FastBitSet& x);

#ifdef DEBUG_LOG
	// Print the bitset.
	void printPretty(FILE* f) const;
	// Print the diffs with the given bitset.
	void printDiff(FILE* f, const FastBitSet& x) const;
	// Print all the '1' bits.
	void printPrettyOnes(FILE* f) const;
	// Print all the '0' bits.
	void printPrettyZeros(FILE* f) const;
#endif
};

// ----------------------------------------------------------------------------
// Inlines.

//
// Set all the bits in this bitset.
//
inline void FastBitSet::set()
{
	PR_ASSERT(sizeInWords);

	Uint32* ptr = bits;
	Int32 count = sizeInWords - 1;
	register Uint32 ones = Uint32(~0);

	do {
		ptr[count] = ones;
	} while(--count >= 0);
}

//
// Clear all the bits in this bitset.
//
inline void FastBitSet::clear()
{
	PR_ASSERT(sizeInWords);

	Uint32* ptr = bits;
	Int32 count = sizeInWords - 1;
	register Uint32 zeros = Uint32(0);

	do {
		ptr[count] = zeros;
	} while(--count >= 0);
}

//
// Count the number of bits at '1'.
//
inline Uint32 FastBitSet::countOnes()
{
	Uint32 counter = 0;
	for (Int32 i = firstOne(); !done(i); i = nextOne(i))
		counter++;

	return counter;
}


//
// Count the number of bits at '0'.
//
inline Uint32 FastBitSet::countZeros()
{
	Uint32 counter = 0;
	for (Int32 i = firstZero(); !done(i); i = nextZero(i))
		counter++;

	return counter;
}

inline void FastBitSet::sizeToAndClear(Pool& pool, Uint32 sizeInBits)
{
	sizeTo(pool, sizeInBits); 
	clear();
}

inline FastBitSet::FastBitSet(Pool& pool, Uint32 sizeInBits) : 
	sizeInWords(0) 
{
	sizeToAndClear(pool, sizeInBits);
}

inline FastBitSet::FastBitSet(Pool& pool, Uint32* bits, Uint32 sizeInBits) : 
	sizeInWords(0) 
{
	sizeTo(pool, sizeInBits);
	PR_ASSERT(sizeInWords);
	Int32 count = sizeInWords - 1;
	Uint32* dst = bits;
	do {
		dst[count] = bits[count];
	} while (--count >= 0);
}



inline FastBitSet::FastBitSet(Uint32* bits, Uint32 sizeInBits) : 
	bits(bits), sizeInWords(getNumberOfWords(sizeInBits)) 
{
#ifdef DEBUG_LOG
	this->sizeInBits = sizeInBits;
#endif /* DEBUG_LOG */
}

#endif /* _FAST_BITSET_H_ */
