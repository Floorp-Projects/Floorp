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
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
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

#ifndef __GCHashtable__
#define __GCHashtable__

namespace MMgc
{
	class GCHashtableIterator;
	/**
	* simplified version of avmplus hashtable, doesn't shrink or handle deletions for instance
	*/
	class GCHashtable
	{
		friend class GCHashtableIterator;
	public:
		const static uint32 kDefaultSize=16;
		const static void * DELETED;
		GCHashtable(unsigned int capacity=kDefaultSize);
		~GCHashtable();
		const void *get(const void *key);
		const void *get(int key) { return get((const void*)key); }
		const void *remove(const void *key);
		// updates value if present, adds and grows if necessary if not
		void put(const void *key, const void *value);
		void add(const void *key, const void *value) { put(key, value); }
		void add(int key, const void *value) { put((const void*)key, value); }
		int count() { return numValues; }

		int nextIndex(int index);
		const void *keyAt(int index) { return table[index<<1]; }
		const void *valueAt(int index) { return table[((index)<<1)+1]; }

	private:
		// capacity
		unsigned int tableSize;

		// size of table array
		unsigned int numValues;

		// number of delete items
		unsigned int numDeleted;

		// table elements
		const void **table;

		static int find(const void *key, const void **table, unsigned int tableSize);
		void grow();
	};

	class GCHashtableIterator
	{
	public:
		GCHashtableIterator(GCHashtable * _ht) : ht(_ht), index(-2) {}

		const void *nextKey() 
		{ 
			do {
				index+=2;
			} while(index < (int)ht->tableSize && ht->table[index] <= GCHashtable::DELETED);
			if(index < (int)ht->tableSize)
				return ht->table[index];
			return NULL;
		}

		const void *value() 
		{ 
			GCAssert(ht->table[index] != NULL); 
			return ht->table[index+1]; 
		}

		void remove()
		{
			GCAssert(ht->table[index] != NULL);			
			ht->table[index] = (const void*)GCHashtable::DELETED;
			ht->table[index+1] = NULL;
		}
	private:
		GCHashtable *ht;
		int index;
	};
}

#endif

