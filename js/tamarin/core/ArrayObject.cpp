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


#include "avmplus.h"

namespace avmplus
{
	ArrayObject::ArrayObject(VTable *vtable, ScriptObject* proto, uint32 capacity)
		: ScriptObject(vtable, proto, 0),
		m_denseArr(capacity)
	{
		AvmAssert(traits()->sizeofInstance >= sizeof(ArrayObject));
		m_length = 0;
		m_lowHTentry = NO_LOW_HTENTRY;
	}

#if 0 // Test code to determine if our array is a pure dense array
	bool ArrayObject::getDense()
	{
		// If are dense part equals are length and we have no 
		// atoms in our HT, we are a pure dense array.  We can't
		// call getTable()->GetSize() since we might have deleted
		// atoms in our HT and size would be non-zero.
		return (isSimpleDense() && !ScriptObject::nextNameIndex(0));
	}
#endif

	// This routine checks to see if our dense portion is directly next
	// to any entries in our HT.  If so, the HT entries are deleted and added
	// to the dense portion.  If the HT is completely emptied, it is cleared.
	void ArrayObject::checkForSparseToDenseConversion()
	{
		// check for lowHTentry being consumed
		if (m_lowHTentry == NO_LOW_HTENTRY)
			return;

		if (getDenseLength() != m_lowHTentry)
			return;

		AvmCore *core = this->core();
		while (getDenseLength() == m_lowHTentry)
		{
			AvmAssert (ScriptObject::hasUintProperty (m_lowHTentry));

			// Move prop from HT to dense Array. No need to update m_length
			Atom lowHT = ScriptObject::getUintProperty (m_lowHTentry);
			this->m_denseArr.push (lowHT);

			// Delete prop from HT
			ScriptObject::delUintProperty (m_lowHTentry);

			// If our low entry happened to match our length, we're out of HT entries
			// and we can just quit.
			if ((m_lowHTentry + 1) == m_length)
			{
				m_lowHTentry = NO_LOW_HTENTRY;				
			}
			else
			{
				// Find the next integer HT prop and update m_lowHTentry
				// This is tricky.  Our HT section could be huge but very sparse
				// Do we want to linearly walk from index+1 to m_length or do
				// we want to walk the entire HT looking for a low integer value?

				if (ScriptObject::hasUintProperty (m_lowHTentry + 1))
				{
					m_lowHTentry++;
				}
				else
				{
					// assume we don't find an entry
					m_lowHTentry = NO_LOW_HTENTRY;
					int index = ScriptObject::nextNameIndex(0);
					while (index)
					{
						Atom name = ScriptObject::nextName (index);
						uint32 nameIndex; 
						if (core->getIndexFromAtom (name, &nameIndex))
						{
							if ((m_lowHTentry == NO_LOW_HTENTRY) || (nameIndex < m_lowHTentry))
							{
								m_lowHTentry = nameIndex;
							}
						}

						index = ScriptObject::nextNameIndex(index);
					}
				}
			}
		}

		// We're done moving our sparse entries over to our dense part of our array.
		// This may have left a large HT that is now completely empty.  If ScriptObject::nextNameIndex(0)
		// returns 0, we know we have no atoms in our HT and we can clear it.
		if (ScriptObject::nextNameIndex (0) == 0)
			getTable()->reset();
	}

	void ArrayObject::setAtomProperty(Atom name, Atom value)
	{
		if (traits()->needsHashtable)
		{
			AvmCore *core = this->core();
			// Update the array length.
			uint32 index;
			if (core->getIndexFromAtom (name, &index))
			{
				return _setUintProperty (index, value);
			}
			
			if (name == core->klength->atom())
				return setLength(core->toUInt32(value));
		}

		ScriptObject::setAtomProperty(name, value);
	}
	
	void ArrayObject::_setUintProperty(uint32 index, Atom value)
	{
		if (traits()->needsHashtable)
		{
			if (hasDense())
			{
				if (index == getDenseLength())
				{
					this->m_denseArr.push (value);
					if (m_length < getDenseLength())
						m_length = getDenseLength();

					checkForSparseToDenseConversion ();
					return;
				}
				else if (index < getDenseLength())
				{
					this->m_denseArr.setAt (index, value);
					return;
				}
				else 
				{
					// fall through and put the new property into our HT
				}
			}
			// If we're NOT dense yet and setting first element, we can create a dense array
			else if (index == 0)
			{			
				m_denseArr.push (value);
				if (!m_length)
					m_length = 1;
				else
					checkForSparseToDenseConversion ();
				return;
			}

			if (m_length <= index) {
				m_length = index+1;
			}

			if ((m_lowHTentry == NO_LOW_HTENTRY) || (index < m_lowHTentry))
				m_lowHTentry = index;
		}
		// end if (dynamic)

		// If our index value is going to overflow our int atom storage and be
		// converted to a string, do that here instead of calling the
		// SciptObject::setUintProperty which will call ArrayObject::setAtomProperty
		// which will call back into this routine in an infinite loop.
		if (index & ScriptObject::MAX_INTEGER_MASK)
			ScriptObject::setAtomProperty(core()->internUint32(index)->atom(), value);
		else
			ScriptObject::setUintProperty(index, value);
	}

	Atom ArrayObject::getAtomProperty(Atom name) const
	{
		if (traits()->needsHashtable)
		{
			AvmCore *core = this->core();
			if (hasDense())
			{
				uint32 index;
				if (core->getIndexFromAtom (name, &index))
				{
					// if we get here, we have a valid integer index.
					if ((index < getDenseLength()))
						return m_denseArr.getAtFast(index);
				}
			}

			if (name == core->klength->atom())
				return core->intToAtom (getLength());
		}

		return ScriptObject::getAtomProperty(name);
	}

	Atom ArrayObject::_getUintProperty(uint32 index) const
	{
		if (traits()->needsHashtable)
		{
			if (hasDense())
			{
				if ((index < getDenseLength()))
					return m_denseArr.getAtFast(index);
			}
		}

		return ScriptObject::getUintProperty (index);
	}

	Atom ArrayObject::_getIntProperty(int index) const
	{
		if (index >= 0) 
			return _getUintProperty(index);
		else // integer is negative - we must intern it
			return getStringProperty(core()->internInt(index));
	}

	void ArrayObject::_setIntProperty(int index, Atom value)
	{
		if (index >= 0) 
			_setUintProperty(index, value);
		else // integer is negative - we must intern it
			setStringProperty(core()->internInt(index), value);
	}

	// This does NOT affect the length of the array
	bool ArrayObject::deleteAtomProperty(Atom name)
	{
		if (traits()->needsHashtable)
		{
			if (hasDense())
			{
				uint32 index;
				if (core()->getIndexFromAtom (name, &index))
				{
					return delUintProperty(index);
				}
			}
		}

		return ScriptObject::deleteAtomProperty(name);
	}

	bool ArrayObject::delUintProperty(uint32 index)
	{
		// if we get here, we have a valid integer index.
		if (traits()->needsHashtable)
		{
			if ((index < getDenseLength()))
			{
				if (index == (getDenseLength() - 1))
				{
					m_denseArr.pop();
				}
				// We're deleting an element in the middle of our array.  The lower
				// part can be left in the dense array but the upper part needs to
				// get moved to the HT.
				else
				{
					for (uint32 i = index + 1; i < getDenseLength(); i++)
					{
						ScriptObject::setUintProperty (i, m_denseArr.getAtFast(i));
					}
					m_denseArr.splice (index, 0, (getDenseLength() - index), 0);
				}

				return true;
			}
		}
		return ScriptObject::delUintProperty(index);
	}

	bool ArrayObject::getAtomPropertyIsEnumerable(Atom name) const
	{
		if (traits()->needsHashtable)
		{
			if (hasDense())
			{
				uint32 index;
				if (core()->getIndexFromAtom(name, &index))
				{
					// {DontEnum} is not supported on the dense portion
					// of an array.  Those properties are always enumerable.
					if (index < getDenseLength())
						return true;
				}
			}
		}

		return ScriptObject::getAtomPropertyIsEnumerable(name);
	}
	
	bool ArrayObject::hasAtomProperty(Atom name) const
	{
		if (traits()->needsHashtable)
		{
			if (hasDense())
			{
				uint32 index;
				if (core()->getIndexFromAtom (name, &index))
				{
					if (index < getDenseLength())
						return true;
				}
			}
		}

		return ScriptObject::hasAtomProperty(name);
	}

	bool ArrayObject::hasUintProperty(uint32 index) const
	{
		if (traits()->needsHashtable)
		{
			if (hasDense())
			{
				if (index < getDenseLength())
					return true;
			}
		}

		return ScriptObject::hasUintProperty (index);
	}

	// Iterator support - for in, for each
	Atom ArrayObject::nextName(int index)
	{
		AvmAssert(index > 0);

		int denseLength = (int)getDenseLength();
		if (index <= denseLength)
		{
			AvmCore *core = this->core();
 			return core->intToAtom(index-1);
		}
		else
		{
			return ScriptObject::nextName (index - denseLength);
		}
	}

	Atom ArrayObject::nextValue(int index)
	{
		AvmAssert(index > 0);

		int denseLength = (int) getDenseLength();
		if (index <= denseLength)
		{
			return m_denseArr.getAtFast (index-1);
		}
		else
		{
			return ScriptObject::nextValue (index - denseLength);
		}
	}

	int ArrayObject::nextNameIndex(int index)
	{
		int denseLength = (int) getDenseLength();
		if (index < denseLength)
		{
			return index + 1;
		}
		else
		{
			index = ScriptObject::nextNameIndex (index - denseLength);
			if (!index)
				return index;
			return denseLength + index;
		}
	}

#ifdef AVMPLUS_VERBOSE
	Stringp ArrayObject::format(AvmCore* core) const
	{
		Stringp prefix = core->newString("[]@");
		return core->concatStrings(prefix, core->formatAtomPtr(atom()));
	}
#endif

	void ArrayObject::setLength(uint32 newLength)
	{
		if (traits()->needsHashtable)
		{
			// Delete all items between size and newLength
			uint32 oldLength = getLength();
			if (newLength < oldLength) 
			{
				uint32 deleteStart = newLength;
				uint32 denseLength = getDenseLength();
				if (newLength < denseLength)
				{
					this->m_denseArr.splice (deleteStart, 0, (denseLength - deleteStart), 0);
					deleteStart = denseLength;
				}

				for (uint32 i = deleteStart; i < oldLength; i++) {
					delUintProperty(i);
				}
			}
			m_length = newLength;
		}
		// else, if !dynamic ignore set.
	}

	// public native function pop(...rest):Object
	Atom ArrayObject::pop()
	{
		if (isSimpleDense())
		{
			if (!m_length)
				return undefinedAtom;

			m_length--;
			return m_denseArr.pop ();
		}

		if (getLength() != 0)
		{
			Atom outAtom = _getUintProperty(getLength()-1);
			setLength(getLength()-1);
			return outAtom;
		} 
		else
		{
			return undefinedAtom;
		}
	}

	uint32 ArrayObject::push(Atom* argv, int argc)
	{
		if (isSimpleDense())
		{
			m_denseArr.push (argv, argc);
			m_length += argc;
		}
		else
		{
			for (int i=0; i < argc; i++) {
				_setUintProperty(getLength(), argv[i]);
			}
		}
		return m_length;
	}

	uint32 ArrayObject::unshift(Atom* argv, int argc)
	{
		if (argc != 0) 
		{
			if (isSimpleDense())
			{
				m_denseArr.unshift (argv, argc);
				m_length += argc;
			}
			else
			{
				uint32 i;
				// First, move all the elements up
				uint32 len = getLength();
				for (i=len; i > 0; ) {  // note: i is unsigned, can't check if --i >=0.
					i--;
					_setUintProperty(i+argc, _getUintProperty(i));
				}
				
				for (i=0; i < ((uint32)argc); i++) {
					_setUintProperty(i, argv[i]);
				}

				setLength(len+argc);
			}
		}

		return getLength();
	}

#ifdef DEBUGGER
	uint32 ArrayObject::size() const
	{
		if(isSimpleDense())
		{
			return ScriptObject::size() + getLength()*sizeof(Atom);
		}
		return ScriptObject::size();
	}
#endif
}
