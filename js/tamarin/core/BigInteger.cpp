/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 1993-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */



#include <math.h>

#include "avmplus.h"
#include "BigInteger.h"

#define X86_MATH
namespace avmplus
{


void BigInteger::setFromDouble(double value)
{
	int e;
	uint64 mantissa = MathUtils::frexp(value,&e); // value = mantissa*2^e, mantissa and e are integers.	
	numWords = (2 + ((e > 0 ? e : -e) /32));

	AvmAssert(numWords <= kMaxBigIntegerBufferSize);
	wordBuffer[1] = (uint32)(mantissa >> 32);
	wordBuffer[0] = (uint32)(mantissa & 0xffffffff);
	numWords = (wordBuffer[1] == 0 ? 1 : 2);
	
	if(e < 0)
		rshiftBy(-e);
	else
		lshiftBy(e);
}

void BigInteger::setFromBigInteger(const BigInteger* from, int32 offset, int32 amount)
{
	numWords = amount;
	AvmAssert(numWords <= kMaxBigIntegerBufferSize);
	memcpy( (byte*)wordBuffer, 
			(byte*)&(from->wordBuffer[offset]), 
			amount*sizeof(uint32));
}

double BigInteger::doubleValueOf() const
{
	// todo:  there's got to be a quicker/smaller code alg for this.
	if (numWords == 1)
		return (double)wordBuffer[0];

	// determine how many of bits are used by the top word
	int bitsInTopWord=1;
	for(uint32 topWord = wordBuffer[numWords-1]; topWord > 1; topWord >>= 1)
		bitsInTopWord++;

	// start result with top word.  We will accumulate the most significant 53 bits of data into it.
	int nextWord = numWords-1;

	// used for rounding:
	bool bit53 = false;
	bool bit54 = false;
	bool rest = false;

	const uint64 ONEL = 1UL;
	uint64 resultMantissa = 0;
	uint64 w = 0;
	int pos = 53;
	int bits = bitsInTopWord;
	int wshift = 0;
	while(pos > 0)
	{
		// extract word and | it in 
		w = wordBuffer[nextWord--];
		resultMantissa |= (w >> wshift);
		pos -= bits;

		if (pos > 0)
		{
			if (nextWord > -1)
			{
				// ready for next word
				bits =   (pos > 31) ? 32 : pos;
				wshift = (pos > 31) ? 0  : 32-bits;
				resultMantissa <<= bits;
			}
			else
			{
				break; // not enough data for full 53 bits
			}
		}
	}

	// rounding 
	if (pos > 0)
	{
		;  // insufficient data for rounding 
	}
	else
	{
		bit53 = ( resultMantissa & 0x1 ) ? true : false;
		if (bits == 32)
		{
			// last extract was exactly 32 bits, so rest of bits are in next word if there is one
			if (nextWord > -1)
			{
				w = wordBuffer[nextWord--];
				bit54 = ( w & (ONEL<<31) ) ? true : false;
				rest = ( w & ((ONEL<<31)-1) ) ? true : false;
			}
		}
		else
		{
			// we know bit54 is in this word but the rest of the data may be in the next
			AvmAssert(bits < 32 && wshift > 0);
			bit54 = ( w & (ONEL<<(wshift-1)) ) ? true : false;

			if (wshift > 1)
				rest = ( w & ((ONEL<<(wshift-1))-1) ) ? true : false;

			// pick up any residual bits
			if (nextWord > -1)
				rest = rest || (wordBuffer[nextWord--] != 0);
		}
	}

	/**
	 * ieee rounding is to nearest even value (bit54 is 2^-1)
	 *   x...1 1..     -> round up since odd (but53 set) 
	 *   x...0 1...1.. -> round up since value > 1/2
	 */
	if (bit54 && (bit53 || rest))
		resultMantissa += 1;

	double result=0;
	int32  expBase2 = lg2() + 1 - 53; // if negative, then we've already captured all the data in the mantissaResult and we can ignore.
	result = (double)resultMantissa;
	if (expBase2 > 0)
	{
		if (expBase2 < 64)
		{
			uint64 uint64_1 = 1;
			result *= (double)(uint64_1 << expBase2);
		}
		else
		{
			result *= MathUtils::pow(2,expBase2);
		}
	}

	return result;
}

// Compare this vs other, return -1 if this < other, 0 if this == other, or 1 if this > other.
//  (todo: optimization for D2A would be to check if comparison was larger than an argument value
//  currently, we add the argument to this and compare the result with other).
int32 BigInteger::compare(const BigInteger *other) const
{
	// if a is bigger than other, subtract has to be positive (assuming unsigned value)
	int result = 0;
	if (numWords > other->numWords)
		result = 1;
	else if (numWords < other->numWords)
		result = -1;
	else
	{
		// otherwise, they are they same number of uint32 words.  need to check numWords by numWords
		for(int x = numWords-1; x > -1 ; x--)
		{
			if (wordBuffer[x] != other->wordBuffer[x])
			{
				result = (wordBuffer[x] < other->wordBuffer[x]) ? -1 : 1;
				break;
			}
		}
	}
	return result;
}

// Multiply by an integer factor and add an integer increment.  The addition is essentially free, 
//  use this with a zero second argument for basic multiplication.
void BigInteger::multAndIncrementBy(int32 factor, int32 addition)	
{
	uint64 opResult;

	// perform mult op, moving carry forward.
	uint64 carry = addition; // init cary with first addition
	int x;
	for(x=0; x < numWords; x++)
	{
		opResult = (uint64)wordBuffer[x] * (uint64)factor + carry;
		carry = opResult >> 32;
		wordBuffer[x] = (uint32)(opResult & 0xffffffff);
	}

	// if carry goes over the existing top, may need to expand wordBuffer
	if (carry) 
	{
		setNumWords(numWords+1);
		wordBuffer[x] = (uint32)carry; 
	}

}

// Multiply by another BigInteger.  If optional arg result is not null, reuse it for
//  result, else allocate a new result.  
//  (note, despite the name "smallerNum", argument does not need to be smaller than this).
BigInteger* BigInteger::mult(const BigInteger* smallerNum, BigInteger* result) const
{
	// Need to know which is the bigger number in terms of number of words.
	const BigInteger *biggerNum = this;
	if (biggerNum->numWords < smallerNum->numWords) 
	{
		const BigInteger *temp = biggerNum;
		biggerNum = smallerNum;
		smallerNum = temp;
	}

	// Make sure result is big enough, initialize with zeros
	int maxNewNumWords = biggerNum->numWords + smallerNum->numWords;
	result->setNumWords(maxNewNumWords); // we'll trim the excess at the end.

	// wipe entire buffer (initToZero flag in setNumWords only sets new elements to zero)
	for(int x = 0; x < maxNewNumWords; x++)
		result->wordBuffer[x] = 0;

	// do the math like gradeschool.  To optimize, use an FFT (http://numbers.computation.free.fr/Constants/Algorithms/fft.html)
	for(int x = 0; x < smallerNum->numWords; x++) 
	{
		uint64 factor = (uint64) smallerNum->wordBuffer[x];
		if (factor) // if 0, nothing to do.
		{
			uint32* pResult = result->wordBuffer+x; // each pass we rewrite elements of result offset by the pass iteration
			uint64  product;
			uint64  carry = 0;

			for(int y = 0; y < biggerNum->numWords; y++)
			{
				product = (biggerNum->wordBuffer[y] * factor) + *pResult + carry;
				carry = product >> 32;
				*pResult++ = (uint32)(product & 0xffffffff);
			}
			*pResult = (uint32)carry;
		}
	}

	// remove leading zeros
	result->trimLeadingZeros();
	
	return result;
}

// Divide this by divisor, put the remainder (i.e. this % divisor) into residual.  If optional third argument result
//  is not null, use it to store the result of the div, else allocate a new BigInteger for the result.
//   Note: this has been hard to get right.  If bugs do show up, use divideByReciprocalMethod instead
//   Note2: this is optimized for the types of numerator/denominators generated by D2A.  It will not work when
//    the result would be a value bigger than 9.  For general purpose BigInteger division, use divideByReciprocalMethod.
BigInteger* BigInteger::quickDivMod(const BigInteger* divisor, BigInteger* residual, BigInteger* result)
{
	// handle easy cases where divisor is >= this
	int compareTo = this->compare(divisor);
	if (compareTo == -1) 
	{
		residual->copyFrom(this);
		result->setValue(0);
		return result;
	}
	else if (compareTo == 0)
	{
		residual->setValue(0);
		result->setValue(1);
		return result;
	}

	int dWords = divisor->numWords;
	/*  this section only necessary for true division instead of special case division needed by D2A.  We are
	     assuming the result is a single digit value < 10 and > -1
	int next   = this->numWords - dWords;
	residual->copyFrom(this, next, dWords); // residual holds a divisor->numWords sized chunk of this.
	*/
	residual->copyFrom(this,0,numWords);
	BigInteger decrement;
	decrement.setFromInteger(0);
	result->setNumWords(divisor->numWords, true);
	uint64 factor;

	//do // need to loop over dword chunks of residual to make this handle division of any arbitrary bigIntegers
	{
		// guess largest factor that * divisor will fit into residual
		factor = (uint64)(residual->wordBuffer[residual->numWords-1]) / divisor->wordBuffer[dWords-1];
		if ( ((factor <= 0) || (factor > 10))   // over estimate of 9 could be 10
			 && residual->numWords > 1 && dWords > 1)
		{
			uint64 bigR = ( ((uint64)residual->wordBuffer[residual->numWords-1]) << 32) 
							 + (residual->wordBuffer[residual->numWords-2]);
			factor =  bigR / divisor->wordBuffer[dWords-1];
			if (factor > 9)  
			{				  //  Note:  This only works because of the relative size of the two operands
							  //   which the D2A class produces.  todo: try generating a numerator out of the
							  //   the top 32 significant bits of residual (may have to get bits from two seperate words)
							  //   and a denominator out of the top 24 significant bits of divisor and use them for
							  //   the factor guess.  Same principal as above applied to 8 bit units.  
				factor = 9;
				/*
				BigInteger::free(decrement);
				return divideByReciprocalMethod(divisor, residual, result);
				*/
			}
		}
		if (factor)
		{
			decrement.copyFrom(divisor);
			decrement.multAndIncrementBy( (uint32)factor,0); 
			// check for overestimate
			// fix bug 121952: must check for larger overestimate, which
			// can occur despite the checks above in some rare cases.
			// To see this case, use: 
			//		this=4398046146304000200000000
			//		divisor=439804651110400000000000
			while (decrement.compare(residual) == 1 && factor > 0)
			{
				decrement.decrementBy(divisor);
				factor--;
			}

			// reduce dividend (aka residual) by factor*divisor, leave remainder in residual
			residual->decrementBy(&decrement);
		}

		// check for factor 0 underestimate
		int comparedTo = residual->compare(divisor);
		if (comparedTo == 1) // correct guess if its an off by one estimate
		{
			residual->decrementBy(divisor);
			factor++;
		}
		
		result->wordBuffer[0] = (uint32)factor;

	/* The above works for the division requirements of D2A, where divisor is always around 10 larger
		than the dividend and the result is always a digit 1-9. 
	    To make this routine work for general division, the following would need to be fleshed out /debugged.
		residual->trimLeadingZeros();

		// While we have more words to divide by and the residual has less words than the
		//  divisor, 
		if (--next >= 0)
		{
			do 
			{
				result->lshiftBy(32);  // shift current result over by a word
				residual->lshiftBy(32);// shift remainder over by a word
				residual->wordBuffer[0] = this->wordBuffer[next]; // drop next word of "this" into bottom word of remainder
			} 
			while(residual->numWords < dWords);
		}

	} while(next >= 0);
	*/
	} 

	// trim zeros off top of residual
	result->trimLeadingZeros();
	return result;
}

/*  Was hoping dividing via a Newton's approximation of the reciprocal would be faster, but
     its not (makes D2A about twice as slow!).  Its not the 32bit divide above that's slow,
	 its how many times you have to iterate over entire BigIntegers.  Below uses a shift,
	 and three multiplies:
*/
BigInteger* BigInteger::divideByReciprocalMethod(const BigInteger* divisor, BigInteger* residual, BigInteger* result)
{
	// handle easy cases where divisor is >= this
	int compareTo = this->compare(divisor);
	if (compareTo == -1) 
	{
		residual->copyFrom(this);
		result->setValue(0);
		return result;
	}
	else if (compareTo == 0)
	{
		residual->setValue(0);
		result->setValue(1);
		return result;
	}

	uint32 d2Prec = divisor->lg2();
	uint32 e = 1 + d2Prec;
	uint32 ag = 1;
	uint32 ar = 31 + this->lg2() - d2Prec;
	BigInteger u; 
	u.setFromInteger(1);

	BigInteger ush;
	ush.setFromInteger(1);

	BigInteger usq;
	usq.setFromInteger(0);

	while (1)
	{
		u.lshift(e + 1,&ush);
		divisor->mult(&u,&usq); // usq = divisor*u^2
		usq.multBy(&u);
		ush.subtract(&usq, &u); // u = ush - usq;

		int32 ush2 = u.lg2(); // ilg2(u);
		e *= 2;
		ag *= 2;
		int32 usq2 = 4 + ag;
		ush2 -= usq2; // BigInteger* diff = ush->subtract(usq); // ush -= usq;  ush > 0
		if (ush2 > 0) // (ush->compare(usq) == 1) 
		{
			u.rshiftBy(ush2); // u >>= ush;
			e -= ush2;
		}
		if (ag > ar) 
			break;
	}
	result = this->mult(&u,result);			// mult by reciprocal (scaled by e)
	result->rshiftBy(e);					// remove scaling	

	BigInteger temp;
	temp.setFromInteger(0);
	divisor->mult(result, &temp);  // compute residual as this - divisor*result
	this->subtract(&temp,residual);  //  todo: should be a more optimal way of doing this

	// ... doesn't work, e is the wrong scale for this (too large)....
	// residual = this->lshift(e,residual);	// residual = (this*2^e - result) / 2^e
	// residual->decrementBy(result);
	// residual->rshiftBy(e);					// remove scaling
	return(result);
}

//  q = this->divBy(divisor)  is equilvalent to:  q = this->quickDivMod(divisor,remainderResult); this = remainderResult; 
BigInteger* BigInteger::divBy(const BigInteger* divisor, BigInteger* divResult)
{
	BigInteger tempInt;
	tempInt.setFromInteger(0);

	quickDivMod(divisor, &tempInt, divResult);  // this has been hard to get right.  If bugs do show up,
	//divResult = divideByReciprocalMethod(divisor,tempInt,divResult); //  try this version instead  Its slower, but less likely to be buggy
	this->copyFrom(&tempInt);
	return divResult;
}

uint32 BigInteger::lg2() const
{
	uint32 powersOf2 = (numWords-1)*32;
	for(uint32 topWord = wordBuffer[numWords-1]; topWord > 1; topWord >>= 1)
		powersOf2++;
	return powersOf2;
}


// Shift a BigInteger by <shiftBy> bits to left.  If
//  result is not NULL, write result into it, else allocate a new BigInteger. 
BigInteger* BigInteger::lshift(uint32 shiftBy, BigInteger* result) const
{
	// calculate how much larger the result will be
	int numNewWords = shiftBy >> 5; // i.e. div by 32
	int totalWords  = numNewWords + numWords + 1; // overallocate by 1, trim if necessary at end.
	result->setNumWords(totalWords,true);


	// make sure we don't create more words for 0
	if ( numWords == 1 && wordBuffer[0] == 0 )
	{
		result->setValue(0); // 0 << num is still 0
		return result;
	}
	const uint32* pSourceBuff = wordBuffer;
	uint32* pResultBuff = result->wordBuffer;
	for(int x = 0; x < numNewWords; x++)
			*pResultBuff++ = 0;
	// move bits from wordBuffer into result's wordBuffer shifted by (shiftBy % 32)
	shiftBy &= 0x1f;
	if (shiftBy) {
		uint32 carry = 0;
		int    shiftCarry = 32 - shiftBy;
		for(int x=0; x < numWords; x++) 
		{
			*pResultBuff++ = *pSourceBuff << shiftBy | carry;		
			carry =  *pSourceBuff++ >> shiftCarry;
		}
		*pResultBuff = carry; // final carry may add a word
		if (*pResultBuff)
			++totalWords;     // prevent trim of overallocated extra word
		}
	else for(int x=0; x < numWords; x++)  // shift was by a clean multiple of 32, just move words
	{
		*pResultBuff++ = *pSourceBuff++;
	}
	result->numWords = totalWords - 1; // trim over allocated extra word
	return result;
}

// Shift a BigInteger by <shiftBy> bits to right.  If
//  result is not NULL, write result into it, else allocate a new BigInteger. 
//  (todo: might be possible to combine rshift and lshift into a single function
//  with argument specifying which direction to shift, but result might be messy/harder to get right)
BigInteger* BigInteger::rshift(uint32 shiftBy, BigInteger* result) const
{
	int numRemovedWords = shiftBy >> 5; // i.e. div by 32

	// calculate how much larger the result will be
	int totalWords = numWords - numRemovedWords; 
	result->setNumWords(totalWords,true);

	// check for underflow
	if (numRemovedWords > numWords) 
	{
		result->setValue(0); 
		return result;
	}

	// move bits from wordBuffer into result's wordBuffer shifted by (shiftBy % 32)
	uint32* pResultBuff = &(result->wordBuffer[totalWords-1]);
	const uint32* pSourceBuff = &(wordBuffer[numWords-1]);
	shiftBy &= 0x1f;
	if (shiftBy) 
	{
		int shiftCarry = 32 - shiftBy;
		uint32 carry = 0;

		for(int x=totalWords-1; x > -1; x--) 
		{
			*pResultBuff-- = *pSourceBuff >> shiftBy | carry;
			carry =  *pSourceBuff-- << shiftCarry;
		}
	}
	else for(int x=totalWords-1; x > -1; x--)   // shift was by a clean multiple of 32, just move words
	{
		*pResultBuff-- = *pSourceBuff--;
	}
	result->numWords = totalWords;
	result->trimLeadingZeros(); // shrink numWords down to correct value
	return result;
}

//  Add or subtract two BigIntegers.  If subtract, we assume the argument is smaller than this since we
//   don't support negative numbers.  If optional third argument result is not null, reuse it for result
//   else allocate a new BigInteger for the result.
BigInteger *BigInteger::addOrSubtract(const BigInteger* smallerNum, bool isAdd, BigInteger* result) const
{
	// figure out which operand is the biggest in terms of number of words.
	int comparedTo = compare(smallerNum);
	const BigInteger* biggerNum = this; 
	if (comparedTo < 0) 
	{
		const BigInteger* temp = biggerNum;
		biggerNum = smallerNum;
		smallerNum = temp;
		AvmAssert(isAdd || comparedTo >= 0); // the d2a/a2d code should never subtract a larger from a smaller.  
											 //  all of the BigInteger code assumes positive numbers.  Proper
											 //  handling would be to create a negative result here (and check for
											 //  negative results everywhere).
	}
	
	result->setNumWords(biggerNum->numWords+1, true);

	if (!isAdd && !comparedTo) // i.e. this - smallerNum == 0
	{
		result->setValue(0);
		return result;
	}

	// do the math: loop over common words of both numbers, performing - or + and carrying the  
	//  overflow/borrow forward to next word.
	uint64 borrow = 0;
	uint64 x;
	int index;
	for( index = 0; index < smallerNum->numWords; index++)
	{
		if (isAdd)
			x = ((uint64)biggerNum->wordBuffer[index]) + smallerNum->wordBuffer[index] + borrow;
		else
			x = ((uint64)biggerNum->wordBuffer[index]) - smallerNum->wordBuffer[index] - borrow; // note x is unsigned.  Ok even if result would be negative however
		borrow = x >> 32 & (uint32)1;
		result->wordBuffer[index] = (uint32)(x & 0xffffffff);
	}

	// loop over remaining words of the larger number, carying borrow/overflow forward
	for( ; index < biggerNum->numWords; index++)
	{
		if (isAdd)
			x = ((uint64)biggerNum->wordBuffer[index]) + borrow;
		else
			x = ((uint64)biggerNum->wordBuffer[index]) - borrow;
		borrow = x >> 32 & (uint32)1;
		result->wordBuffer[index] = (uint32)(x & 0xffffffff);
	}

	// handle final overflow if this is add
	if (isAdd && borrow)
	{
		result->wordBuffer[index] = (uint32)(borrow & 0xffffffff);
		index++;
	}
	// loop backwards over result, removing leading zeros from our numWords count
	while( !(result->wordBuffer[--index]) ) 
	{	
	}
	result->numWords = index+1;

	return result;

}

}
