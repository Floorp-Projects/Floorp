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

#ifndef __avmplus_Hashtable__
#define __avmplus_Hashtable__


namespace avmplus
{
	/**
	 * common base class for hashtable-like objects.
	 */
	class Hashtable : public MMgc::GCFinalizedObject
	{
	public:
		/**
		 * since identifiers are always interned strings, they can't be 0,
		 * so we can use 0 as the empty value.
		 */
		const static Atom EMPTY = 0;

		/** DELETED is stored as the key for deleted items5 */
		const static Atom DELETED = undefinedAtom;

		/** kDefaultCapacity must be a power of 2 */
		const static int kDefaultCapacity = 8;

		int getSize() const { return size; }
		int getNumAtoms() const { return logNumAtoms ? 1<<(logNumAtoms-1) : 0; }

		bool hasDontEnumSupport() const { return flags & kDontEnumSupport; }
		void setDontEnumSupport(bool flag) { flags = (short)((flags & ~kDontEnumSupport) | (flag ? kDontEnumSupport : 0)); }

		int dontEnumMask() const { return hasDontEnumSupport() ? kDontEnumBit : 0; }
		
		bool getAtomPropertyIsEnumerable(Atom name) const;
		void setAtomPropertyIsEnumerable(Atom name, bool enumerable);

		// kDontEnumBit is or'd into atoms to indicate that the property is {DontEnum}
		enum
		{
			kDontEnumBit = 1
		};

	private:
		int size;
		short logNumAtoms;
		short flags;

		// Flags used in the "flags" variable
		enum
		{
			kDontEnumSupport = 1,
			kHasDeletedItems = 2
		};

		bool hasDeletedItems() const { return (flags & kHasDeletedItems) != 0; }

		void setNumAtoms(int numAtoms)
		{
			logNumAtoms = numAtoms ? (short)(FindOneBit(numAtoms)+1) : 0;
			AvmAssert(getNumAtoms() == numAtoms);
		}

#if defined(AVMPLUS_IA32) || defined(AVMPLUS_AMD64)
		static inline uint32 FindOneBit(uint32 value)
		{
#ifndef __GNUC__
			_asm
			{
				bsr eax,[value];
			}
#else
			// DBC - This gets rid of a compiler warning and matchs PPC results where value = 0
			register int	result = ~0;
			
			if (value)
			{
				asm (
					"bsr %1, %0"
					: "=r" (result)
					: "m"(value)
					);
			}
			return result;
#endif
		}
	#elif defined(AVMPLUS_PPC)

		static inline int FindOneBit(uint32 value)
		{
			register int index;
			#ifdef DARWIN
			asm ("cntlzw %0,%1" : "=r" (index) : "r" (value));
			#else
			register uint32 in = value;
			asm { cntlzw index, in; }
			#endif
			return 31-index;
		}

		#else // generic platform

		static int FindOneBit(uint32 value)
		{
			for (int i=0; i < 32; i++)
				if (value & (1<<i))
					return i;
			// asm versions of this function are undefined if no bits are set
			AvmAssert(false);
			return 0;
		}

#endif
		
		void setAtoms(Atom *atoms);

	protected:

		/** property hashtable, this has no DWB on purpose, setAtoms contains the WB */
		Atom* atoms;

	public:

		Atom* getAtoms() { return atoms; }

		/**
		 * initialize with a known capacity.  i.e. we can fit minSize
		 * elements in without rehashing.
		 * @param heap
		 * @param capacity  # of logical slots
		 */
		Hashtable(MMgc::GC *gc, int capacity = kDefaultCapacity)
		{
			initialize(gc, capacity);
		}
	
		void destroy(); 

		void reset()
		{
			MMgc::GC *gc = MMgc::GC::GetGC(atoms);
			destroy();
			initialize(gc);
		}
		
		void initialize(MMgc::GC *gc, int capacity = kDefaultCapacity);

		virtual ~Hashtable();

		/* See CPP for load factor variants. */
		bool isFull() const;
		
		/**
		 * @name operations on name/value pairs
		 */
		/*@{*/
		void put(Atom name, Atom value);
		Atom get(Atom name) const;
		Atom remove(Atom name);

		bool contains(Atom name) const
		{
			return (atoms[find(name, atoms, getNumAtoms())] & ~dontEnumMask()) == name;
		}
		/*@}*/

		/**
		 * convenience functions for storing pointers as values.  This is
		 * safe for internal tables that always store the same kind of
		 * pointer.
		 */
		/*@{*/
		void put(Atom name, const void *value)
		{
			put(name,(Atom)value);
		}

		void add(Atom name, const void *value)
		{
			add(name, (Atom)value);
		};
		/*@}*/

		/**
		 * Finds the hash bucket corresponding to the key x
		 * in the hash table starting at t, containing tLen
		 * atoms.
		 *
		 * This is a quadratic probe, but we only hit even-numbered
		 * slots since those hold keys.
		 */
		int find(Atom x, const Atom *t, unsigned tLen) const;
		int find(Stringp x, const Atom *t, unsigned tLen) const;
		int find(Atom x) const { return find(x, atoms, getNumAtoms()); }

		/**
		 * Adds a name/value pair to a hash table.  Automatically
		 * grows the hash table if it is full.
		 */
		void add (Atom name, Atom value);

		/**
		 * Called to grow the Hashtable, particularly by add.
		 *
		 * - Calculates the needed size for the new Hashtable
		 *   (typically 2X the current size)
		 * - Creates a new array of Atoms
		 * - Rehashes the current table into the new one
		 * - Deletes the old array of Atoms and sets the Atom
		 *   pointer to our new array of Atoms
		 */
		void grow();

		/**
		 * Allow caller to enumerate all entries in the table.
		 */
		int next(int index);
		Atom keyAt(int index);
		Atom valueAt(int index);
		//void removeAt(int index);

		void setHasDeletedItems(bool val) { flags = (short)((flags & ~kHasDeletedItems) | (val ? kHasDeletedItems : 0)); }

	protected:
		int rehash(Atom *oldAtoms, int oldlen, Atom *newAtoms, int newlen);
	};

	/** 
	 * If key is an object weak ref's are used
	 */
	class WeakKeyHashtable : public Hashtable
	{
	public:
		WeakKeyHashtable (MMgc::GC* gc): Hashtable(gc) { };
		void add(Atom key, Atom value);
		Atom get(Atom key) { return Hashtable::get(getKey(key)); }
		void remove(Atom key) { Hashtable::remove(getKey(key)); }
		bool contains(Atom key) { return Hashtable::contains(getKey(key)); }
	private:
		Atom getKey(Atom key);
		void prune();
	};	

	/** 
	 * If key is an object weak ref's are used
	 */
	class WeakValueHashtable : public Hashtable
	{
	public:
		WeakValueHashtable (MMgc::GC* gc): Hashtable(gc) { };
		void add(Atom key, Atom value);
		Atom get(Atom key) { return getValue(key, Hashtable::get(key)); }
		Atom remove(Atom key) { return getValue(key, Hashtable::remove(key)); }
	private:
		Atom getValue(Atom key, Atom value);
		void prune();
	};	
}

#endif /* __avmplus_Hashtable__ */
