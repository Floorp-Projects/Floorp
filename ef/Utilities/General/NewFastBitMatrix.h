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
#include "Pool.h"

class FastBitSet;

// ----------------------------------------------------------------------------
// FastBitMatrix
//
// !!!! A FastBitMatrix cannot have a null number of rows nor a null 
// !!!! number of column.
//

class FastBitMatrix
{
private:

	Uint32* bits;								// Array of words to store the bits.
	Uint32  sizeInWords;						// Number of words allocated.
	Uint8   rowShift;							// A row is (1 << shift) words wide.
	Uint32  rowSizeInWords;						// Number of used words in a row.
#ifdef DEBUG_LOG
	Uint32  nCols;								// Number of column in this matrix.
#endif
  

	FastBitMatrix(const FastBitMatrix&);		// No copy constructor.

	// Return the offset to the first word on this row.
	Uint32 getRowOffset(Uint32 row) const {return row << rowShift;}
	// Return the offset to word containing column from the begining of the row.
	Uint32 getWordOffsetFromBeginOfRow(Uint32 column) const {return column >> 5;}
	// Return the offset of the word containing (row, column).
	Uint32 getWordOffset(Uint32 row, Uint32 column) const {return getRowOffset(row) + getWordOffsetFromBeginOfRow(column);}
	// Return the offset of the bit a column from the begining of the word containing column.
	Uint32 getBitOffset(Uint32 column) const {return column & 31;}

public:
	// This is not a valid bitmatrix. The method sizeTo must be called before any
	// use of this instance.
	FastBitMatrix() : sizeInWords(0), rowShift(0) {}

	// Allocate a new matrix.
	inline FastBitMatrix(Pool& pool, Uint32 nRows, Uint32 nCols);

	// Resize the matrix.
	void sizeTo(Pool& pool, Uint32 nRows, Uint32 nCols);
	// Resize the matrix and clear all bits.
	inline void sizeToAndClear(Pool& pool, Uint32 nRows, Uint32 nCols);

	// Return a pointer to the row bits.
	inline Uint32* getRowBits(Uint32 row) const {return &bits[getRowOffset(row)];}

	// Clear all the bits in the matrix.
	inline void clear();
	// Clear the bit at (row, column).
	void clear(Uint32 row, Uint32 column) {PR_ASSERT(sizeInWords); bits[getWordOffset(row, column)] &= ~(1<<getBitOffset(column));}
	// Clear the row
	void clear(Uint32 row);
	// Set all the bits in the matrix.
	inline void set();
	// Set the bit at (row, column).
	void set(Uint32 row, Uint32 column) {PR_ASSERT(sizeInWords); bits[getWordOffset(row, column)] |= (1<<getBitOffset(column));}
	// Set the row.
	void set(Uint32 row);
	// Test the bit at (row, column).
	bool test(Uint32 row, Uint32 column) const {PR_ASSERT(sizeInWords); return (bits[getWordOffset(row, column)]>>getBitOffset(column))&1;}

	// And rows src1 & src2 to row dst
	void andRows(Uint32 src1, Uint32 src2, Uint32 dst);
	// And row src & the bitset x to row dst
	void andRows(Uint32 src, const FastBitSet& x, Uint32 dst);
	// Or rows src1 & src2 to row dst
	void orRows(Uint32 src1, Uint32 src2, Uint32 dst);
	// Or row src & the bitset x to row dst
	void orRows(Uint32 src, const FastBitSet& x, Uint32 dst);
	// Copy row src to row dst.
	void copyRows(Uint32 src, Uint32 dst);
	// Copy the bitset x to the row dst.
	void copyRows(const FastBitSet& x, Uint32 dst);
	// Copy the row x to the bitset x.
	void copyRows(Uint32 src, FastBitSet& x);
	// Compare the rows x and y.
	bool compareRows(Uint32 x, Uint32 y) const;

	// Arithmetic operators.
	FastBitMatrix& operator |= (const FastBitMatrix& x);
	FastBitMatrix& operator &= (const FastBitMatrix& x);
	FastBitMatrix& operator ^= (const FastBitMatrix& x);
	FastBitMatrix& operator -= (const FastBitMatrix& x);

	// Comparison operators.
	friend bool operator == (const FastBitMatrix& x, const FastBitMatrix& y);
	friend bool operator != (const FastBitMatrix& x, const FastBitMatrix& y) {return !(x == y);}

	// Copy operator.
	FastBitMatrix& operator = (const FastBitMatrix& x);
  
#ifdef DEBUG_LOG
	// Print the matrix.
	void printPretty(FILE* f) const;
	// Print the diffs with the given matrix.
	void printDiff(FILE* f, const FastBitMatrix& x) const;
#endif /* DEBUG_LOG */
};

// ----------------------------------------------------------------------------
// Inlines


//
// Set all the bits in the matrix.
//
inline void FastBitMatrix::set()
{
	PR_ASSERT(sizeInWords);

	Uint32* ptr = bits;
	Int32 count = sizeInWords - 1;
	register Uint32 ones = Uint32(~0);

	do 
		ptr[count] = ones;
	while(--count >= 0);
}

//
// Clear all the bits in the matrix.
//
inline void FastBitMatrix::clear()
{
	PR_ASSERT(sizeInWords);

	Uint32* ptr = bits;
	Int32 count = sizeInWords - 1;
	register Uint32 zeros = Uint32(0);

	do 
		ptr[count] = zeros;
	while(--count >= 0);
}

//
// Resize the matrix and clear all the bits.
//
inline void FastBitMatrix::sizeToAndClear(Pool& pool, Uint32 nRows, Uint32 nCols)
{
	sizeTo(pool, nRows, nCols);
	clear();
}

//
// Resize the new matrix and clear its content.
//
inline FastBitMatrix::FastBitMatrix(Pool& pool, Uint32 nRows, Uint32 nCols) : sizeInWords(0) 
{
	PR_ASSERT(nRows && nCols);
	sizeToAndClear(pool, nRows, nCols);
}


#endif /* _FAST_BITMATRIX_H_ */

