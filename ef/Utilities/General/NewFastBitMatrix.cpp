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
#include "FastBitMatrix.h"
#include "FastBitSet.h"
#include "Pool.h"
#include "prbit.h" // for PR_CEILING_LOG2


//
// Resize the matrix. Allocate the words from the given pool.
//
void FastBitMatrix::sizeTo(Pool& pool, Uint32 nRows, Uint32 nCols)
{
	PR_ASSERT(nRows && nCols);

	Uint32 rowSizeInWords = (nCols + 31) >> 5;
	this->rowSizeInWords = rowSizeInWords;
	Uint8 shift;

	PR_CEILING_LOG2(shift, rowSizeInWords);
 
	if (shift) // The biggest square matrix we can allocate is Matrix(0x7ffe0,0x7ffe0) (524256).
		PR_ASSERT((nRows & ~((1 << (32 - shift)) - 1)) == 0); // Overflow should send a signal (or exception).

	Uint32 newSizeInWords = nRows << shift;
	if (newSizeInWords > sizeInWords) {
		sizeInWords = newSizeInWords;
		bits = new(pool) Uint32[sizeInWords];
	}
	rowShift = shift;
#ifdef DEBUG_LOG
	this->nCols = nCols;
#endif /* DEBUG_LOG */
}

//
// And rows src1 & src2 to row dst
//
void FastBitMatrix::andRows(Uint32 src1, Uint32 src2, Uint32 dst)
{
	PR_ASSERT(sizeInWords);

	Uint32* srcPtr1 = &bits[getRowOffset(src1)];
	Uint32* srcPtr2 = &bits[getRowOffset(src2)];
	Uint32* dstPtr  = &bits[getRowOffset(dst)];
	Uint32* limit = &dstPtr[rowSizeInWords];

	while (dstPtr < limit)
		*dstPtr++ = *srcPtr1++ & *srcPtr2++;
}

//
// And rows src1 & src2 to row dst
//
void FastBitMatrix::andRows(Uint32 src, const FastBitSet& x, Uint32 dst)
{
	PR_ASSERT(sizeInWords && (rowSizeInWords == x.sizeInWords));

	Uint32* srcPtr1 = &bits[getRowOffset(src)];
	Uint32* srcPtr2 = x.bits;
	Uint32* dstPtr  = &bits[getRowOffset(dst)];
	Uint32* limit = &srcPtr2[x.sizeInWords];

	while (srcPtr2 < limit)
		*dstPtr++ = *srcPtr1++ & *srcPtr2++;
}

//
// Or rows src1 & src2 to row dst
//
void FastBitMatrix::orRows(Uint32 src1, Uint32 src2, Uint32 dst)
{
	PR_ASSERT(sizeInWords);

	Uint32* srcPtr1 = &bits[getRowOffset(src1)];
	Uint32* srcPtr2 = &bits[getRowOffset(src2)];
	Uint32* dstPtr  = &bits[getRowOffset(dst)];
	Uint32* limit = &dstPtr[rowSizeInWords];

	while (dstPtr < limit)
		*dstPtr++ = *srcPtr1++ | *srcPtr2++;
}

//
// Or rows src1 & src2 to row dst
//
void FastBitMatrix::orRows(Uint32 src, const FastBitSet& x, Uint32 dst)
{
	PR_ASSERT(sizeInWords && (rowSizeInWords == x.sizeInWords));

	Uint32* srcPtr1 = &bits[getRowOffset(src)];
	Uint32* srcPtr2 = x.bits;
	Uint32* dstPtr  = &bits[getRowOffset(dst)];
	Uint32* limit = &srcPtr2[x.sizeInWords];

	while (srcPtr2 < limit)
		*dstPtr++ = *srcPtr1++ | *srcPtr2++;
}

//
// Compare the rows x and y. Return true if they are equal.
//
bool FastBitMatrix::compareRows(Uint32 x, Uint32 y) const
{
	PR_ASSERT(sizeInWords);

	Uint32* ptr1 = &bits[getRowOffset(x)];
	Uint32* ptr2 = &bits[getRowOffset(y)];
	Uint32* limit = &ptr1[rowSizeInWords];

	while (ptr1 < limit)
		if (*ptr1++ != *ptr2++)
			return false;
	return true;
}

//
// Copy the row src to the row dst.
// 
void FastBitMatrix::copyRows(Uint32 src, Uint32 dst)
{
	PR_ASSERT(sizeInWords);

	Uint32* srcPtr = &bits[getRowOffset(src)];
	Uint32* dstPtr = &bits[getRowOffset(dst)];
	Uint32* limit = &srcPtr[rowSizeInWords];

	while (srcPtr < limit)
		*dstPtr++ = *srcPtr++;
}

//
// Copy the bitset x to the row dst.
//
void FastBitMatrix::copyRows(const FastBitSet& x, Uint32 dst)
{
	PR_ASSERT(sizeInWords && x.sizeInWords == rowSizeInWords);

	Uint32* srcPtr = x.bits;
	Uint32* dstPtr = &bits[getRowOffset(dst)];
	Uint32* limit = &srcPtr[x.sizeInWords];

	while (srcPtr < limit)
		*dstPtr++ = *srcPtr++;
}

//
// Copy the row x to the bitset x.
//
void FastBitMatrix::copyRows(Uint32 src, FastBitSet& x)
{
	PR_ASSERT(sizeInWords && x.sizeInWords == rowSizeInWords);

	Uint32* srcPtr = &bits[getRowOffset(src)];
	Uint32* dstPtr = x.bits;
	Uint32* limit = &dstPtr[x.sizeInWords];

	while (dstPtr < limit)
		*dstPtr++ = *srcPtr++;
}

//
// Clear the row
//
void FastBitMatrix::clear(Uint32 row)
{
	PR_ASSERT(sizeInWords);

	Uint32* ptr = &bits[getRowOffset(row)];
	Int32 count = rowSizeInWords - 1;
	register Uint32 zeros = Uint32(0);

	do
		ptr[count] = zeros;
	while(--count >= 0);
}

//
// Set the row
//
void FastBitMatrix::set(Uint32 row)
{
	PR_ASSERT(sizeInWords);

	Uint32* ptr = &bits[getRowOffset(row)];
	Int32 count = rowSizeInWords - 1;
	register Uint32 ones = Uint32(~0);

	do 
		ptr[count] = ones;
	while(--count >= 0);
}


//
// Arithmetic operators.
//

#define FastBitMatrixOperation(x, op, val)						\
PR_ASSERT(sizeInWords && sizeInWords == x.sizeInWords);			\
	Int32 nRows = (sizeInWords >> rowShift) - 1;				\
	do {														\
		Int32 count = rowSizeInWords - 1;						\
		Uint32* dst = &bits[getRowOffset(nRows)];				\
		Uint32* src = &x.bits[x.getRowOffset(nRows)];			\
		do {													\
			Uint32 word = src[count];							\
			if (word != val)									\
				dst[count] op word;								\
		} while(--count >= 0);									\
	} while(--nRows >= 0);

FastBitMatrix& FastBitMatrix::operator |= (const FastBitMatrix& x)
{
	FastBitMatrixOperation(x, |=, 0);
	return *this;
}

FastBitMatrix& FastBitMatrix::operator &= (const FastBitMatrix& x)
{
	FastBitMatrixOperation(x, &=, Uint32(~0));
	return *this;
}

FastBitMatrix& FastBitMatrix::operator ^= (const FastBitMatrix& x)
{
	FastBitMatrixOperation(x, ^=, 0);
	return *this;
}

FastBitMatrix& FastBitMatrix::operator -= (const FastBitMatrix& x)
{
	FastBitMatrixOperation(x, &= ~, 0);
	return *this;
}

#undef FastBitMatrixOperation

//
// Return true if the 2 matrix are equals.
//
bool operator == (const FastBitMatrix& x, const FastBitMatrix& y)
{
	Uint32 nRows = x.sizeInWords >> x.rowShift;

	if ((y.sizeInWords >> y.rowShift) != nRows)
		return false;

	Uint32* ptr1 = x.bits;
	Uint32* ptr2 = y.bits;
	Uint32* limit = &ptr1[x.sizeInWords];

	while (ptr1 < limit)
		if (*ptr1++ != *ptr2++)
			return false;

	return true;
}

//
// Copy the bits from the bitmatrix x.
//
FastBitMatrix& FastBitMatrix::operator = (const FastBitMatrix& x)
{
	Uint32 nRows = x.sizeInWords >> x.rowShift;

	PR_ASSERT((nRows == (sizeInWords >> rowShift)) && (rowSizeInWords == x.rowSizeInWords));

	Uint32* src = x.bits;
	Uint32* dst = bits;
	Uint32* limit = &dst[sizeInWords];

	while (dst < limit)
		*dst++ = *src++;

	rowSizeInWords = x.rowSizeInWords;
	rowShift = x.rowShift;

	return *this;
}

#ifdef DEBUG_LOG
//
// Print the matrix.
//
void FastBitMatrix::printPretty(FILE* f) const
{
	PR_ASSERT(sizeInWords);

	fprintf(f, "[\n");
	Uint32 nRows = sizeInWords >> rowShift;
	for (Uint32 i = 0; i < nRows; i++) {
		fprintf(f, " ");
		FastBitSet row(&bits[getRowOffset(i)], nCols);
		row.printPretty(f);
	}
	fprintf(f, "]\n");
}

//
// Print the diffs.
//
void FastBitMatrix::printDiff(FILE *f, const FastBitMatrix& x) const
{
	Uint32 nRows = sizeInWords >> rowShift;
	PR_ASSERT((x.nCols == nCols) && (nRows == (x.sizeInWords >> x.rowShift)));

	fprintf(f, "[\n");
	for (Uint32 i = 0; i < nRows; i++) {	
		fprintf(f, " ");
		FastBitSet row(&bits[i << rowShift], nCols);
		FastBitSet xRow(&x.bits[i << rowShift], nCols);
		row.printDiff(f, xRow);
	}
	fprintf(f, "]\n");
}

#endif /* DEBUG_LOG */
