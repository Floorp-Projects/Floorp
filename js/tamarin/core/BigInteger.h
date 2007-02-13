/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 1993-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef __avmplus_BigInteger__
#define __avmplus_BigInteger__

//
//  BigInteger is an implementation of arbitrary precision integers.  This class is used by the
//   D2A class in mathutils.h to print doubles to greater than 15 digits of precision.  It has
//   been implemented with that use in mind, so if you want to use it for some other purpose
//   be aware that certain basic features have not been implemented: divide() assumes the result
//   is a single digit, there is no support for negative values, and subtract() assumes that the
//   term being subtracted from is larger than the operand.
//
//  By far, memory allocation is the gating speed factor.  D2A never uses more than 8 or so values
//   during a given run, so a simple memoryCache is used to keep 8 temporaries around for quick reuse.
//   This would likely have to be adjusted for more general use.
namespace avmplus
{
	class BigInteger
	{
		public:

			// Memory management
			// --------------------------------------------------------
			BigInteger()
			{
				numWords = 0;
			}

			~BigInteger()
			{
			}

			inline void setFromInteger(int32 initVal)
			{
				wordBuffer[0] = initVal;
				numWords = 1;
			}
			void setFromDouble(double value);
			void setFromBigInteger(const BigInteger* from,  int32 offset,  int32 amount);

			double doubleValueOf() const; // returns double approximation of this integer


			// operations
			// --------------------------------------------------------
			
			// Compare (sum = this+offset)  vs other.  if sum > other, return 1.  if sum < other,
			//  return -1, else return 0.  Note that all terms are assumed to be positive.
			int32 compare(const BigInteger *other) const;

			// same as above, but compare this+offset with other.
			int32 compareOffset(const BigInteger *other, const BigInteger *offset)
			{
				BigInteger tempInt;
				tempInt.setFromInteger(0);
				add(offset,&tempInt); 
				return tempInt.compare(other);
			}


			// Add or subtract one BigInteger from another.  isAdd determines if + or - is performed.
			//  If result is specified, write result into it (allows for reuse of temporaries), else
			//  allocate a new BigInteger for the result.
			BigInteger* addOrSubtract(const BigInteger* other, bool isAdd, BigInteger* result) const;

			// syntactic sugar for simple case of adding two BigIntegers
			inline BigInteger* add(const BigInteger* other, BigInteger* result) const
			{ 
				return addOrSubtract(other, true, result); 
			}
			// syntactic sugar for simple case of subtracting two BigIntegers
			inline BigInteger* subtract(const BigInteger* other, BigInteger* result) const
			{ 
				return addOrSubtract(other, false, result); 
			}

			// Increment this by other.  other is assumed to be positive
			inline void incrementBy(const BigInteger* other)
			{
				BigInteger tempInt;
				tempInt.setFromInteger(0);
				addOrSubtract(other,true,&tempInt);
				copyFrom(&tempInt);
			}

			// Decrement this by other. other is assumed to be positive and smaller than this.
			inline void decrementBy(const BigInteger* other)
			{
				BigInteger tempInt;
				tempInt.setFromInteger(0);
				addOrSubtract(other,false,&tempInt);
				copyFrom(&tempInt);
			}

			// Shift a BigInteger by <shiftBy> bits to right or left.  If
			//  result is not NULL, write result into it, else allocate a new BigInteger. 
			BigInteger* rshift(uint32 shiftBy, BigInteger* result) const;
			BigInteger* lshift(uint32 shiftBy, BigInteger* result) const;

			// Same as above but store result back into "this"
			void lshiftBy(uint32 shiftBy)
			{
				BigInteger tempInt;
				tempInt.setFromInteger(0);
				lshift(shiftBy,&tempInt);
				copyFrom(&tempInt);
			}

			void rshiftBy(uint32 shiftBy)
			{
				BigInteger tempInt;
				tempInt.setFromInteger(0);
				rshift(shiftBy,&tempInt);
				copyFrom(&tempInt);
			}

			// Multiply this by an integer factor and add an integer increment.  Store result back in this.
			//  Note, the addition is essentially free
			void multAndIncrementBy( int32 factor,  int32 addition);
			inline void multBy( int32 factor) 
			{ 
				multAndIncrementBy(factor,0); 
			}

			// Shorthand for multiplying this by a double valued factor (and storing result back in this).
			inline void multBy(double factor)
			{
				BigInteger bigFactor;
				bigFactor.setFromDouble(factor);
				multBy(&bigFactor);
			}

			// Multiply by another BigInteger.  If optional arg result is not null, reuse it for
			//  result, else allocate a new result.  
			//  (note, despite the name "smallerNum", argument does not need to be smaller than this).
			BigInteger* mult(const BigInteger* other, BigInteger* result) const;
			void multBy(const BigInteger *other) 
			{
				BigInteger tempInt;
				tempInt.setFromInteger(0);
				mult(other,&tempInt);
				copyFrom(&tempInt);
			}

			// divide this by divisor, put the remainder (i.e. this % divisor) into residual.  If optional third argument result
			//  is not null, use it to store the result of the div, else allocate a new BigInteger for the result.
			//   Note: this has been hard to get right.  If bugs do show up, use divideByReciprocalMethod instead
			//   Note2: this is optimized for the types of numerator/denominators generated by D2A.  It will not work when
			//    the result would be a value bigger than 9.  For general purpose BigInteger division, use divideByReciprocalMethod.
			BigInteger* quickDivMod(const BigInteger* divisor, BigInteger* modResult, BigInteger* divResult);
			
			/* Thought this would be faster than the above, but its not */
			BigInteger* divideByReciprocalMethod(const BigInteger* divisor, BigInteger* modResult, BigInteger* divResult);
			uint32 lg2() const;		
			
				

			//  q = this->divBy(divisor)  is equilvalent to:  q = this->divMod(divisor,remainderResult); this = remainderResult; 
			BigInteger* divBy(const BigInteger* divisor, BigInteger* divResult);

			//  copy words from another BigInteger.  By default, copy all words into this.  If copyOffsetWords
			//   is not -1, then copy numCopyWords starting at word copyOffsetWords.
			void copyFrom(const BigInteger *other,  int32 copyOffsetWords=-1,  int32 numCopyWords=-1)
			{
				 int32 numCopy = (numCopyWords == -1) ? other->numWords : numCopyWords;
				 int32 copyOffset = (copyOffsetWords == -1) ? 0 : copyOffsetWords;

				copyBuffer(other->wordBuffer+copyOffset, numCopy);
			}

			//  Data members
			// --------------------------------------------------------
			static const int kMaxBigIntegerBufferSize=128;
			uint32 wordBuffer[kMaxBigIntegerBufferSize];
			int32  numWords;

		private:
			//  sign;  Simplifications are made assuming all numbers are positive

			// Utility methods
			// --------------------------------------------------------

			// set value to a simple integer
			inline void setValue(uint32 val) 
			{
				this->numWords = 1;
				this->wordBuffer[0] = val;
			}


			// copy wordBuffer from another BigInteger
			inline void copyBuffer(const uint32 *newBuff,  int32 size)
			{
				numWords = size;
				AvmAssert(newBuff != wordBuffer);
				AvmAssert(numWords <= kMaxBigIntegerBufferSize);
				memcpy(wordBuffer, newBuff, numWords*sizeof(uint32));
			}
			
			inline void setNumWords( int32 newNumWords,bool initToZero=false)
			{
				int32 oldNumWords = numWords;
				numWords = newNumWords;
				AvmAssert(numWords <= kMaxBigIntegerBufferSize);
				if (initToZero && oldNumWords < numWords)
				{
					for( int32 x = oldNumWords-1; x < numWords; x++)
						wordBuffer[x] = 0;
				}
			}

			// remove leading zero words from number. 
			inline void trimLeadingZeros() 
			{ 
				int32 x;
				for( x = numWords-1; x >= 0 && wordBuffer[x] == 0; x--)
					;
				this->numWords = (x == -1) ? 1 : x+1; // make sure a zero value has numWords = 1
			}
	};

}
#endif // __avmplus_BigInteger__

