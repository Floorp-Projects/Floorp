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
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
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

#ifndef __avmplus_BitSet__
#define __avmplus_BitSet__


namespace avmplus
{
	/**
	 * Bit vectors are an efficent method of keeping True/False information 
	 * on a set of items or conditions. Class BitSet provides functions 
	 * to manipulate individual bits in the vector.
	 *
	 * Since most vectors are rather small an array of longs is used by
	 * default to house the value of the bits.  If more bits are needed
	 * then an array is allocated dynamically outside of this object. 
	 * 
	 * This object is not optimized for a fixed sized bit vector
	 * it instead allows for dynamically growing the bit vector.
	 */ 
	class BitSet: public MMgc::GCFinalizedObject
	{
		public:

			enum {  kUnit = 8*sizeof(long),
					kDefaultCapacity = 4   };

			BitSet()
			{
				capacity = kDefaultCapacity;
				for(int i=0; i<capacity; i++)
					bits.ar[i] = 0;
			}

			~BitSet()
			{
				if (capacity > kDefaultCapacity)
					delete [] bits.ptr;
				capacity = 0;
			}

			void set(int bitNbr)
			{
				int index = bitNbr / kUnit;
				int bit = bitNbr % kUnit;
				if (index >= capacity)
					grow(index+1);

				if (capacity > kDefaultCapacity)
					bits.ptr[index] |= (1<<bit);
				else
					bits.ar[index] |= (1<<bit);
			}

			void clear(int bitNbr)
			{
				int index = bitNbr / kUnit;
				int bit = bitNbr % kUnit;
				if (index < capacity)
				{
					if (capacity > kDefaultCapacity)
						bits.ptr[index] &= ~(1<<bit);
					else
						bits.ar[index] &= ~(1<<bit);
				}
			}

			bool get(int bitNbr) const
			{
				int index = bitNbr / kUnit;
				int bit = bitNbr % kUnit;
				bool value = false;
				if (index < capacity)
				{
					if (capacity > kDefaultCapacity)
						value = ( bits.ptr[index] & (1<<bit) ) ? true : false;
					else
						value = ( bits.ar[index] & (1<<bit) ) ? true : false;
				}
				return value;
			}

		private:

			// Grow the array until at least newCapacity big
			void grow(int newCapacity)
			{
				// create vector that is 2x bigger than requested 
				newCapacity *= 2;
				//MEMTAG("BitVector::Grow - long[]");
				long* newBits = new long[newCapacity];

				// copy the old one 
				for(int i=0; i<capacity; i++)
				{
					if (capacity > kDefaultCapacity)
						newBits[i] = bits.ptr[i];
					else
						newBits[i] = bits.ar[i];
				}

				// clear out the rest
				for(int i=capacity; i<newCapacity; i++)
					newBits[i] = 0;

				// in with the new out with the old
				if (capacity > kDefaultCapacity)
					delete [] bits.ptr;

				bits.ptr = newBits;
				capacity = newCapacity;
			}

			// by default we use the array, but if the vector 
			// size grows beyond kDefaultCapacity we allocate
			// space dynamically.
			int capacity;
			union
			{
				long   ar[kDefaultCapacity];
				long*  ptr;
			}
			bits;
	};
}

#endif /* __avmplus_BitSet__ */
