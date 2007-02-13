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


#ifndef __avmplus_AtomArray__
#define __avmplus_AtomArray__

namespace avmplus
{
	class AtomArray
	{
	public:
		AtomArray(int initialCapacity = 0);
		~AtomArray();

		/////////////////////////////////////////////////////
		// Array AS API support
		/////////////////////////////////////////////////////

		// Pop last element off the end of the array and shrink length
		Atom pop();

		// n arguments are pushed on the array (argc is returned)
		uint32 push(Atom *args, int argc);

		// Reverse array elements
		void reverse();

		// return 0th element, shift rest down
		Atom shift();

		// insertPoint arg - place to insert
		// insertCount arg - number to insert
		// deleteCount - number to delete
		// args - #insertCount args to insert
		// offset - offset in args to start the insertion from.
		void splice(uint32 insertPoint, uint32 insertCount, uint32 deleteCount, AtomArray *args, int offset=0);

		// insert array of arguments at front of array
		Atom unshift(Atom *args, int argc);

		/////////////////////////////////////////////////////

		uint32 getLength() const { return m_length; };
		void setLength(uint32 len) { m_length = len; }
		uint32 capacity()  const 
		{ 
			return MMgc::GC::Size(m_atoms)/sizeof(Atom);
		}

		void push (Atom a);
		void push (const AtomArray *a);
		void removeAt (uint32 index);

		void insert (uint32 index, Atom a);
		void setAt (uint32 index, Atom a);

		void clear();

		Atom operator[](uint32 index) const { return getAt(index); };
		Atom getAt(uint32 index) const;

		// ONLY USE THIS IF YOU PRE-CHECK THE LENGTH 
		Atom getAtFast(uint32 index) const 
		{ 
			AvmAssert(index < m_length); 
			return m_atoms[index]; 
		};

		/**
		 * Compacts the AtomArray so it is exactly large enough
		 * to hold the contents of the array.
		 *
		 * This is useful for conserving memory.
		 */
		void pack();

		void checkCapacity (int newLength);

	private:
		uint32	m_length;
		Atom* m_atoms;

		const static int  kMinCapacity = 4;

		void setAtInternal(uint32 index, Atom a)
		{
			// m_arr[index] = a;
			WBATOM( MMgc::GC::GetGC(m_atoms), m_atoms, m_atoms + index, a);
		}

		void setAtoms(MMgc::GC *gc, Atom *atoms)
		{
			WB(gc, gc->FindBeginning(this), &m_atoms, atoms);
		}
	};
}

#endif /* __avmplus_AtomArray__ */
