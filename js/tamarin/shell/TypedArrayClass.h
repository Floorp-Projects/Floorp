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


#ifndef TYPEDARRAYCLASS_INCLUDED
#define TYPEDARRAYCLASS_INCLUDED

namespace avmshell
{
	template <class T>
	class TypedArrayObject : public ScriptObject
	{
	public:
		TypedArrayObject(VTable *ivtable, ScriptObject *delegate)
			: ScriptObject(ivtable, delegate)
		{
			m_array = NULL;
			m_capacity = 0;
			m_length = 0;
		}

		~TypedArrayObject()
		{
			delete [] m_array;
			m_array = NULL;
			m_capacity = 0;
			m_length = 0;
		}

		virtual bool hasAtomProperty(Atom name) const
		{
			return ScriptObject::hasAtomProperty(name) || getAtomProperty(name) != undefinedAtom;
		}
		
		virtual void setAtomProperty(Atom name, Atom value)
		{
			uint32 index;
			if (core()->getIndexFromAtom(name, &index))
			{
				setUintProperty(index, value);
			}
			else
			{
				ScriptObject::setAtomProperty(name, value);
			}
		}
		
		virtual Atom getAtomProperty(Atom name) const
		{
			uint32 index;
			if (core()->getIndexFromAtom(name, &index))
			{
				return getUintProperty(index);
			}
			else
			{
				return ScriptObject::getAtomProperty(name);
			}
		}
			
		virtual Atom getUintProperty(uint32 index) const
		{
			if (m_length <= index)
			{
				return undefinedAtom;
			}
			else
			{
				return valueToAtom(m_array[index]);
			}
		}
		
		virtual void setUintProperty(uint32 index, Atom value)
		{
			if (m_length <= index)
			{
				grow(index+1);
				m_length = index+1;
			}
			atomToValue(value, m_array[index]);
		}			

		uint32 get_length()
		{
			return m_length;
		}
		
		void set_length(uint32 newLength)
		{
			if (newLength > m_capacity)
			{
				grow(newLength);
			}
			m_length = newLength;
		}

	private:
		uint32 m_capacity;
		uint32 m_length;
		T *m_array;

		enum { kGrowthIncr = 4096 };

		void atomToValue(Atom atom, sint16& value)
		{
			value = (sint16) core()->integer(atom);						
		}
		void atomToValue(Atom atom, uint16& value)
		{
			value = (uint16) AvmCore::integer_u(atom);			
		}
		void atomToValue(Atom atom, sint32& value)
		{
			value = core()->integer(atom);						
		}
		void atomToValue(Atom atom, uint32& value)
		{
			value = AvmCore::integer_u(atom);			
		}
		void atomToValue(Atom atom, float& value)
		{
			value = (float) AvmCore::number_d(atom);
		}
		void atomToValue(Atom atom, double& value)
		{
			value = AvmCore::number_d(atom);
		}

		Atom valueToAtom(sint16 value) const
		{
			return core()->intToAtom(value);
		}
		Atom valueToAtom(uint16 value) const
		{
			return core()->uintToAtom(value);
		}
		Atom valueToAtom(sint32 value) const
		{
			return core()->intToAtom(value);
		}
		Atom valueToAtom(uint32 value) const
		{
			return core()->uintToAtom(value);
		}
		Atom valueToAtom(float value) const
		{
			return core()->doubleToAtom((double)value);
		}
		Atom valueToAtom(double value) const
		{
			return core()->doubleToAtom(value);
		}

		void grow(uint32 newCapacity)
		{
			if (newCapacity > m_capacity)
			{
				newCapacity = ((newCapacity+kGrowthIncr)/kGrowthIncr)*kGrowthIncr;
				T *newArray = new T[newCapacity];
				if (!newArray)
				{
					toplevel()->throwError(kOutOfMemoryError);
				}
				if (m_array)
				{
					memcpy(newArray, m_array, m_length * sizeof(T));
					delete [] m_array;
				}
				memset(newArray+m_length, 0, (newCapacity-m_capacity) * sizeof(T));
				m_array = newArray;
				m_capacity = newCapacity;
			}
		}
	};

	typedef TypedArrayObject<sint16> ShortArrayObject;
	typedef TypedArrayObject<uint16> UShortArrayObject;
	typedef TypedArrayObject<sint32> IntArrayObject;
	typedef TypedArrayObject<uint32> UIntArrayObject;
	typedef TypedArrayObject<float> FloatArrayObject;
	typedef TypedArrayObject<double> DoubleArrayObject;	
	
	class ShortArrayClass : public ClassClosure
	{
    public:
		ShortArrayClass(VTable *vtable);

		ScriptObject *createInstance(VTable *ivtable, ScriptObject *delegate);

		DECLARE_NATIVE_MAP(ShortArrayClass)
    };

	class UShortArrayClass : public ClassClosure
	{
    public:
		UShortArrayClass(VTable *vtable);

		ScriptObject *createInstance(VTable *ivtable, ScriptObject *delegate);

		DECLARE_NATIVE_MAP(UShortArrayClass)
    };

	class IntArrayClass : public ClassClosure
	{
    public:
		IntArrayClass(VTable *vtable);

		ScriptObject *createInstance(VTable *ivtable, ScriptObject *delegate);

		DECLARE_NATIVE_MAP(IntArrayClass)
    };

	class UIntArrayClass : public ClassClosure
	{
    public:
		UIntArrayClass(VTable *vtable);

		ScriptObject *createInstance(VTable *ivtable, ScriptObject *delegate);

		DECLARE_NATIVE_MAP(UIntArrayClass)
    };

	class FloatArrayClass : public ClassClosure
	{
    public:
		FloatArrayClass(VTable *vtable);

		ScriptObject *createInstance(VTable *ivtable, ScriptObject *delegate);

		DECLARE_NATIVE_MAP(FloatArrayClass)
    };

	class DoubleArrayClass : public ClassClosure
	{
    public:
		DoubleArrayClass(VTable *vtable);

		ScriptObject *createInstance(VTable *ivtable, ScriptObject *delegate);

		DECLARE_NATIVE_MAP(DoubleArrayClass)
    };
}	

#endif /* TYPEDARRAYCLASS_INCLUDED */
