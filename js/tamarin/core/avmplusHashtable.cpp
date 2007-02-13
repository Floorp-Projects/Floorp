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


#include "avmplus.h"

using namespace MMgc;

// WARNING:
// WARNING:
// WARNING:
// Never do "MMgc::GC::GetGC(this)" in the HashTable object.  It is dynamically
// placed at the end of a ScriptObject following traits data which can extend
// greater than 4k off the starting pointer.  This will cause GetGC(this) to fail.
// WARNING:
// WARNING:
// WARNING:

namespace avmplus
{	
	void Hashtable::initialize(GC *gc, int capacity)
	{
		capacity = MathUtils::nextPowerOfTwo(capacity);
		setNumAtoms(capacity*2);
		AvmAssert(getNumAtoms());
		MMGC_MEM_TYPE(this);
		setAtoms((Atom *) gc->Alloc (sizeof(Atom) * getNumAtoms(), GC::kContainsPointers|GC::kZero));
		flags = 0;
	}

	Hashtable::~Hashtable()
	{
		destroy();
	}

	void Hashtable::destroy()
	{
		if(atoms) {
			GC *gc = GC::GetGC(atoms);
#ifdef MMGC_DRC
			AvmCore::decrementAtomRegion(atoms, getNumAtoms());
#endif
			gc->Free (atoms);
		}
		atoms = NULL;
		setNumAtoms(0);
		size = 0;
		flags = 0;
	}

	void Hashtable::setAtoms(Atom *newAtoms)
	{
		GC *gc = GC::GetGC(newAtoms);
		gc->writeBarrier(gc->FindBeginning(this), &atoms, newAtoms);
	}

	void Hashtable::put(Atom name, Atom value)
	{
		int i = find(name, atoms, getNumAtoms());
		GC *gc = GC::GetGC(atoms);
		if ((atoms[i] & ~dontEnumMask()) != name) {
			AvmAssert(!isFull());			
			//atoms[i] = name;
			WBATOM(gc, atoms, &atoms[i], name);
			size++;
		}
		//atoms[i+1] = value;
		WBATOM( gc, atoms, &atoms[i+1], value);
	}

    Atom Hashtable::get(Atom name) const
    {
		int i;
		return atoms[i = find(name, atoms, getNumAtoms())] == name ? atoms[i+1] : undefinedAtom;
    }

	int Hashtable::find(Stringp x, const Atom *t, unsigned tLen) const
	{
		AvmAssert(x != NULL);
		return find(x->atom(), t, tLen);
	}

	int Hashtable::find(Atom x, const Atom *t, unsigned m) const
	{
		int mask = ~dontEnumMask();
		x &= mask;
		
		#if 0 // debug code to print out the strings we're searching for
		static int debug =0;
		if (debug && AvmCore::isString(x))
		{
			Stringp s = AvmCore::atomToString(x);
			AvmDebugMsg (s->c_str(), false);
			AvmDebugMsg ("\n", false);
		}
		#endif

		int bitmask = (m - 1) & ~0x1;

		AvmAssert(x != EMPTY && x != DELETED);
        // this is a quadratic probe but we only hit even numbered slots since those hold keys.
        int n = 7 << 1;
		#ifdef _DEBUG
		unsigned loopCount = 0;
		#endif
		// Note: Mask off MSB to avoid negative indices.  Mask off bottom
		// 3 bits because it doesn't contribute to hash.  Double it
		// because names, values stored adjacently.
        unsigned i = ((0x7FFFFFF8 & x)>>2) & bitmask;  
        Atom k;
        while ((k=t[i]&mask) != x && k != EMPTY)
		{
			i = (i + (n += 2)) & bitmask;		// quadratic probe
			AvmAssert(loopCount++ < m);			// don't scan forever
		}
		AvmAssert(i <= ((m-1)&~0x1));
        return i;
	}
		
	Atom Hashtable::remove(Atom name)
	{
        int i = find(name, atoms, getNumAtoms());
		Atom val = undefinedAtom;
        if ((atoms[i]&~dontEnumMask()) == name)
        {
			val = atoms[i+1];
            atoms[i] = DELETED;
			atoms[i+1] = DELETED;
			setHasDeletedItems(true);
        }
		return val;
	}

    int Hashtable::rehash(Atom *oldAtoms, int oldlen, Atom *newAtoms, int newlen)
    {
		int newSize = 0;
        for (int i=0, n=oldlen; i < n; i += 2)
        {
            Atom oldAtom;
            if ((oldAtom=oldAtoms[i]) != EMPTY && oldAtom != DELETED)
            {
                // inlined & simplified version of put()
                int j = find(oldAtom, newAtoms, newlen);
                newAtoms[j] = oldAtom;
                newAtoms[j+1] = oldAtoms[i+1];
				newSize++;
			}
        }

		return newSize;
    }

	/**
		* load factor is 0.75 so we're full if size >= M*0.75
		* where M = atoms.length/2<br>
        *     size >= M*3/4<br>
        *     4*size >= 3*M<br>
        *     4*size >= 3*atoms.length/2<br>
        *     8*size >= 3*atoms.length<br>
        *     size<<3 >= 3*atoms.length
		*/
	/**
		* load factor is 0.9 so we're full if size >= M*0.9
		* where M = atoms.length/2<br>
        *     size >= M*9/10<br>
        *     10*size >= 9*M<br>
        *     10*size >= 9*atoms.length/2<br>
        *     20*size >= 9*atoms.length<br>
		*/
	bool Hashtable::isFull() const 
	{ 
		// This seems to work very well and the Intel compiler converts both the multiplies
		// into shift+add operations.
		return (5*(getSize()+1)) >= 2*getNumAtoms(); // 0.80
		#if 0
		//return ((size<<3)+1) >= 3*getNumAtoms(); // 0.75
		//return ((40*size)+1) >= 19*getNumAtoms(); // 0.95
		//return ((40*size)+1) >= 18*getNumAtoms(); // 0.90
		//return ((40*size)+1) >= 17*getNumAtoms(); // 0.85
		//return ((40*size)+1) >= 15*getNumAtoms(); // 0.75
		//Edwins suggestion - if (size > max - max>>N)) grow  (N = 5, 4, 3, 2)
		//#define SHIFTFACTOR 4
		//NOT CORRECT
		//return ( ( size << SHIFTFACTOR ) + size > (getNumAtoms() << (SHIFTFACTOR-1)) );
		#endif
	}

	void Hashtable::add(Atom name, Atom value)
	{
		if (isFull())
		{
			grow();
		}
		put(name, value);
	}

	void Hashtable::grow()
	{
		// grow the table by 2N+1
		//     new = 2*old+1 ; old == o.atoms.length/2
		//         = 2*(o.atoms.length/2)+1
		//         = o.atoms.length + 1
		// If we have deleted slots, we don't grow our HT because our rehash will clear
		// out spots for us. 
		int capacity = hasDeletedItems() ? getNumAtoms() : MathUtils::nextPowerOfTwo(getNumAtoms()+1);
		GC* gc = GC::GetGC(atoms);
		MMGC_MEM_TYPE(this);
		Atom *newAtoms = (Atom *) gc->Calloc(capacity, sizeof(Atom), GC::kContainsPointers|GC::kZero);
		size = rehash(atoms, getNumAtoms(), newAtoms, capacity);
		gc->Free (atoms);
		setAtoms(newAtoms);
		setNumAtoms(capacity);
		setHasDeletedItems(false);
		return;
	}

	Atom Hashtable::keyAt(int index)
	{
		return atoms[(index-1)<<1];
	}

	Atom Hashtable::valueAt(int index)
	{
		return atoms[((index-1)<<1)+1];
	}

/*	void Hashtable::removeAt(int index)
	{
		int i = (index-1)<<1;
        atoms[i] = DELETED;
		atoms[i+1] = DELETED;
		setHasDeletedItems(true);
	}*/

	// call this method using the previous value returned
	// by this method starting with 0, until 0 is returned.
	int Hashtable::next(int index)
	{
		if (index != 0) {
			index = index<<1;
		}
		// Advance to first non-empty slot.
		int mask = dontEnumMask();
		int numAtoms = getNumAtoms();
		while (index < numAtoms) {
			if ((atoms[index]&7) != EMPTY && (atoms[index]&7) != DELETED && (atoms[index]&mask) != kDontEnumBit) {
				return (index>>1)+1;
			}
			index += 2;
		}
		return 0;
	}

	bool Hashtable::getAtomPropertyIsEnumerable(Atom name) const
	{
		if (hasDontEnumSupport())
		{
			int i = find(name, atoms, getNumAtoms());
			if ((atoms[i]&~kDontEnumBit) == name)
			{
				return (atoms[i]&kDontEnumBit) == 0;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return contains(name);
		}
	}

	void Hashtable::setAtomPropertyIsEnumerable(Atom name, bool enumerable)
	{
		if (hasDontEnumSupport())
		{
			int i = find(name, atoms, getNumAtoms());
			if ((atoms[i]&~kDontEnumBit) == name)
			{
				atoms[i] = (atoms[i]&~kDontEnumBit) | (enumerable ? 0 : kDontEnumBit);
			}
		}
	}
	
	Atom WeakKeyHashtable::getKey(Atom key)
	{
		// this gets a weak ref number, ie double keys, okay I guess
		if(AvmCore::isPointer(key)) {
			GCWeakRef *weakRef = ((GCObject*)(key&~7))->GetWeakRef();
			key = AvmCore::gcObjectToAtom(weakRef);
		}
		return key;
	}

	void WeakKeyHashtable::add(Atom key, Atom value)
	{
		if(isFull()) {
			prune();
			grow();
		}
		put(getKey(key), value);
	}

	void WeakKeyHashtable::prune()
	{
		for(int i=0, n=getNumAtoms(); i<n; i+=2) {
			if(AvmCore::isGCObject(atoms[i])) {
				GCWeakRef *ref = (GCWeakRef*)(atoms[i]&~7);
				if(ref && ref->get() == NULL) {
					// inlined delete
					atoms[i] = DELETED;
					atoms[i+1] = DELETED;
					setHasDeletedItems(true);
				}
			}
		}
	}
	
	Atom WeakValueHashtable::getValue(Atom key, Atom value)
	{
		if(AvmCore::isGCObject(value)) {
			GCWeakRef *wr = (GCWeakRef*)(value&~7);
			if(wr->get() != NULL) {
				// note wr could be a pointer to a double, that's what this is for
				if(GC::GetGC(atoms)->IsRCObject(wr->get())) {
					value = ((AvmPlusScriptableObject*)wr->get())->toAtom();
				} else {
					AvmAssert(false);
				}
			} else {
				remove(key);
				value = undefinedAtom;
			}
		}
		return value;
	}

	void WeakValueHashtable::add(Atom key, Atom value)
	{
		if(isFull()) {
			prune();
			grow();
		}
		if(AvmCore::isPointer(value)) {
			GCWeakRef* wf = ((GCObject*)(value&~7))->GetWeakRef();
			value = AvmCore::gcObjectToAtom(wf);
		}
		put(key, value);
	}

	void WeakValueHashtable::prune()
	{
		for(int i=0, n=getNumAtoms(); i<n; i+=2) {
			if(AvmCore::isPointer(atoms[i+1])) {
				GCWeakRef *ref = (GCWeakRef*)(atoms[i+1]&~7);
				if(ref && ref->get() == NULL) {
					// inlined delete
					atoms[i] = DELETED;
					atoms[i+1] = DELETED;
					setHasDeletedItems(true);
				}
			}
		}
	}
}
