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

#include "avmplus.h"

using namespace MMgc;

namespace avmplus
{	
	MultinameHashtable::MultinameHashtable(int capacity)
	{
		Init(capacity);
	}

	void MultinameHashtable::Init(int capacity)
	{
		if(capacity)
		{
			numTriplets = MathUtils::nextPowerOfTwo(capacity);

			MMgc::GC* gc = MMgc::GC::GetGC(this);

			AvmAssert(numTriplets > 0);
			MMGC_MEM_TYPE(this);
			triples = (Triple *) gc->Alloc (sizeof(Triple) * numTriplets, GC::kContainsPointers|GC::kZero);
		}
	}

	MultinameHashtable::~MultinameHashtable()
	{
		GC *gc = GC::GetGC(this);
		gc->Free (triples);
	}

	bool MultinameHashtable::isFull() const
	{ 
		// 0.80 load factor
		return 5*(size+1) >= numTriplets*4; 
	}

	int MultinameHashtable::find(Stringp name, Namespace* ns, Triple *t, unsigned m)
	{
		AvmAssert(name != NULL && ns != NULL);

        // this is a quadratic probe but we only hit every third slot since those hold keys.
        int n = 7;

		int bitmask = (m - 1);

		// Note: Mask off MSB to avoid negative indices.  Mask off bottom
		// 3 bits because it doesn't contribute to hash.  Triple it
		// because names, namespaces, and values are stored adjacently.
        unsigned i = ((0x7FFFFFF8 & (uintptr)name) >> 3) & bitmask;
        Stringp k;
        while (((k=t[i].name) != name || t[i].ns != ns) && k != NULL)
		{
			i = (i + (n++)) & bitmask;			// quadratic probe
		}
        return i;
	}
		
	void MultinameHashtable::put(Stringp name, Namespace* ns, Binding value)
	{
		AvmAssert(!isFull());

		GC *gc = GC::GetGC(triples);
		int i = find(name, ns, triples, numTriplets);
		Triple &t = triples[i];
		if (t.name == name)
		{
			// This <name,ns> pair is already in the table
			AvmAssert(t.ns == ns);
		}
		else
		{
			// New table entry for this <name,ns> pair
			size++;
			//triples[i] = name;
			WBRC(gc, triples, &t.name, name);
			//triples[i+1] = ns;
			WBRC(gc, triples, &t.ns, ns);
		}

		//triples[i+2] = value;
		WB(gc, triples, &t.value, value);
	}

	Binding MultinameHashtable::get(Stringp name, Namespace* ns) const
	{
		Triple* t = triples;
		int i = find(name, ns, t, numTriplets);
		if (t[i].name == name)
		{
			AvmAssert(t[i].ns == ns);
			return t[i].value;
		}
		return BIND_NONE;
	}

	Binding MultinameHashtable::getName(Stringp name) const
	{
		Triple* t = triples;
		for (int i=0, n=numTriplets; i < n; i++)
		{
			if (t[i].name == name)
			{
				return t[i].value;
			}
		}
		return BIND_NONE;
	}

	Binding MultinameHashtable::get(Stringp mnameName, NamespaceSet *nsset) const
	{
		Binding match = BIND_NONE;
		int nsCount = nsset->size;

        // this is a quadratic probe but we only hit every third slot since those hold keys.
        int n = 7;
		int bitMask = numTriplets - 1;

		// Note: Mask off MSB to avoid negative indices.  Mask off bottom
		// 3 bits because it doesn't contribute to hash.  Triple it
		// because names, namespaces, and values are stored adjacently.
        unsigned i = ((0x7FFFFFF8 & (uintptr)mnameName)>>3) & bitMask;
		int j;
		Stringp atomName;

		Triple* t = triples;
        while ((atomName = t[i].name) != EMPTY)
		{
			if (atomName == mnameName)
			{
				Namespace *ns = t[i].ns;
				for (j=0; j < nsCount; j++)
				{
					if (ns == nsset->namespaces[j])
					{
						if (match == BIND_NONE)
						{
							match = t[i].value;
							break;
						}
						else if (match != t[i].value)
						{
							return BIND_AMBIGUOUS;
						}
					}
				}
			}

			i = (i + (n++)) & bitMask;			// quadratic probe
		}

		return match;
	}

	Binding MultinameHashtable::getMulti(Multiname* mname) const
	{
		// multiname must not be an attr name, have wildcards, or have runtime parts.
		AvmAssert(mname->isBinding() && !mname->isAnyName());

		if (!mname->isNsset())
			return get(mname->getName(), mname->getNamespace());
		else
			return get(mname->getName(), mname->getNsset());
	}

	void MultinameHashtable::add(Stringp name, Namespace* ns, Binding value)
	{
		if (isFull())
		{
			grow();
		}
		put(name, ns, value);
	}

    void MultinameHashtable::rehash(Triple *oldAtoms, int oldTriplet, Triple *newAtoms, int newTriplet)
    {
        for (int i=0, n=oldTriplet; i < n; i++)
        {
            Stringp oldName;
            if ((oldName=oldAtoms[i].name) != EMPTY)
            {
                // inlined & simplified version of put()
                int j = find(oldName, oldAtoms[i].ns, newAtoms, newTriplet);
                newAtoms[j].name = oldName;
                newAtoms[j].ns = oldAtoms[i].ns;
                newAtoms[j].value = oldAtoms[i].value;
			}
        }
    }

	void MultinameHashtable::grow()
	{
		// double our table
		int capacity = numTriplets*2;
		MMgc::GC* gc = MMgc::GC::GetGC(this);
		MMGC_MEM_TYPE(this);
		Triple *newAtoms = (Triple *) gc->Calloc(capacity, sizeof(Triple), GC::kContainsPointers|GC::kZero);
		rehash(triples, numTriplets, newAtoms, capacity);
		gc->Free (triples);
		triples = newAtoms;
		numTriplets = capacity;
	}

	Stringp MultinameHashtable::keyAt(int index)
	{
		AvmAssert(NULL != triples[index-1].name);
		return triples[index-1].name;
	}

	Namespace* MultinameHashtable::nsAt(int index)
	{
		return triples[index-1].ns;
	}

	Binding MultinameHashtable::valueAt(int index)
	{
		return triples[index-1].value;
	}

	// call this method using the previous value returned
	// by this method starting with 0, until 0 is returned.
	int MultinameHashtable::next(int index)
	{
		// Advance to first non-empty slot.
		Triple* t = triples;
		while (index < numTriplets) {
			if (t[index++].name != NULL) {
				return index;
			}
		}
		return 0;
	}	
}
