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



#include "MMgc.h"

namespace MMgc
{
	const void *GCHashtable::DELETED = (const void*)1;

	GCHashtable::GCHashtable(unsigned int capacity)
	{
		tableSize = capacity*2;
		table = new const void*[tableSize];
		memset(table, 0, tableSize * sizeof(void*));
		numValues = 0;
		numDeleted = 0;
	}

	GCHashtable::~GCHashtable()
	{
		delete [] table;
		table = 0;
		tableSize = 0;
		numDeleted = 0;
		numValues = 0;
	}

	void GCHashtable::put(const void *key, const void *value)
	{
		int i = find(key, table, tableSize);
		if (table[i] != key) {
			// .75 load factor, note we don't take numDeleted into account
			// we want to rehash a table with lots of deleted slots
			if(numValues * 8 > tableSize * 3)
			{
				grow();
				// grow rehashes
				i = find(key, table, tableSize);
			}
			table[i] = key;
			numValues++;
		}
		table[i+1] = value;
	}
	
	const void *GCHashtable::remove(const void *key)
	{
		const void *ret = NULL;
		int i = find(key, table, tableSize);
		if (table[i] == key) {
			table[i] = (const void*)DELETED;
			ret = table[i+1];
			table[i+1] = NULL;
			numDeleted++;
		}		
		return ret;
	}

	const void *GCHashtable::get(const void *key)
	{
		return table[find(key, table, tableSize)+1];
	}

	/* static */
	int GCHashtable::find(const void *key, const void **table, unsigned int tableSize)
	{	
		GCAssert(key != (const void*)DELETED);
		int bitmask = (tableSize - 1) & ~0x1;

        // this is a quadratic probe but we only hit even numbered slots since those hold keys.
        int n = 7 << 1;
		#ifdef _DEBUG
		unsigned loopCount = 0;
		#endif
		// Note: Mask off MSB to avoid negative indices.  Mask off bottom
		// 2 bits because of alignment.  Double it because names, values stored adjacently.
        unsigned i = ((0x7FFFFFF8 & (uintptr)key)>>1) & bitmask;  
        const void *k;
        while ((k=table[i]) != key && k != NULL)
		{
			i = (i + (n += 2)) & bitmask;		// quadratic probe
			GCAssert(loopCount++ < tableSize);			// don't scan forever
		}
		GCAssert(i <= ((tableSize-1)&~0x1));
        return i;
	}

	void GCHashtable::grow()
	{
		int newTableSize = tableSize;

		unsigned int occupiedSlots = numValues - numDeleted;
        GCAssert(numValues >= numDeleted);

		// grow or shrink as appropriate:
		// if we're greater than %50 full grow
		// if we're less than %10 shrink
		// else stay the same
		if (4*occupiedSlots > tableSize)
			newTableSize <<= 1;
		else if(20*occupiedSlots < tableSize && 
				tableSize > kDefaultSize)
			newTableSize >>= 1;

		const void **newTable = new const void*[newTableSize];
		memset(newTable, 0, newTableSize*sizeof(void*));

		numValues = 0;
		numDeleted = 0;

        for (int i=0, n=tableSize; i < n; i += 2)
        {
            const void *oldKey;
            if ((oldKey=table[i]) != NULL)
            {
                // inlined & simplified version of put()
				if(oldKey != (const void*)DELETED) {
					int j = find(oldKey, newTable, newTableSize);
					newTable[j] = oldKey;
					newTable[j+1] = table[i+1];
					numValues++;
				}
			}
        }
	
		delete [] table;
		table = newTable;
		tableSize = newTableSize;
	}

	int GCHashtable::nextIndex(int index)
	{
		unsigned int i = index<<1;
		while(i < tableSize)
		{
			if(table[i] > DELETED)
				return (i>>1)+1;
			i += 2;
		}
		return 0;
	}
}

