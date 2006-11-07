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


#include "avmshell.h"

namespace avmshell
{
	BEGIN_NATIVE_MAP(StringBuilderClass)
		NATIVE_METHOD(avmplus_StringBuilder_append, StringBuilderObject::append)
		NATIVE_METHOD(avmplus_StringBuilder_capacity_get, StringBuilderObject::get_capacity)
		NATIVE_METHOD(avmplus_StringBuilder_charAt, StringBuilderObject::charAt)
		NATIVE_METHOD(avmplus_StringBuilder_charCodeAt, StringBuilderObject::charCodeAt)
		NATIVE_METHOD(avmplus_StringBuilder_ensureCapacity, StringBuilderObject::ensureCapacity)
		NATIVE_METHOD(avmplus_StringBuilder_indexOf, StringBuilderObject::indexOf)
		NATIVE_METHOD(avmplus_StringBuilder_insert, StringBuilderObject::insert)
		NATIVE_METHOD(avmplus_StringBuilder_lastIndexOf, StringBuilderObject::lastIndexOf)
		NATIVE_METHOD(avmplus_StringBuilder_length_get, StringBuilderObject::get_length)
		NATIVE_METHOD(avmplus_StringBuilder_length_set, StringBuilderObject::set_length)
		NATIVE_METHOD(avmplus_StringBuilder_remove, StringBuilderObject::remove)
		NATIVE_METHOD(avmplus_StringBuilder_removeCharAt, StringBuilderObject::removeCharAt)
		NATIVE_METHOD(avmplus_StringBuilder_replace, StringBuilderObject::replace)
		NATIVE_METHOD(avmplus_StringBuilder_reverse, StringBuilderObject::reverse)
		NATIVE_METHOD(avmplus_StringBuilder_setCharAt, StringBuilderObject::setCharAt)
		NATIVE_METHOD(avmplus_StringBuilder_substring, StringBuilderObject::substring)
		NATIVE_METHOD(avmplus_StringBuilder_toString, StringBuilderObject::_toString)
		NATIVE_METHOD(avmplus_StringBuilder_trimToSize, StringBuilderObject::trimToSize)
	END_NATIVE_MAP()
	
	StringBuilderObject::StringBuilderObject(VTable *vtable, ScriptObject *delegate)
		: ScriptObject(vtable, delegate)
	{
		m_buffer = new wchar[kInitialCapacity];
		if (!m_buffer)
		{
			toplevel()->throwError(kOutOfMemoryError);
		}
		m_length = 0;
		m_capacity = kInitialCapacity;
	}

	StringBuilderObject::~StringBuilderObject()
	{
		delete [] m_buffer;
		m_buffer = NULL;
		m_length = NULL;
		m_capacity = NULL;
	}
	
	void StringBuilderObject::append(Atom value)
	{
		Stringp str = core()->string(value);
		int len = str->length();

		if (len > 0)
		{
			ensureCapacity(m_length + len);
			memcpy(m_buffer+m_length, str->c_str(), len*sizeof(wchar));
			m_length += len;
		}
	}

	uint32 StringBuilderObject::get_capacity()
	{
		return m_capacity;
	}
	
	Stringp StringBuilderObject::charAt(uint32 index)
	{
		if (index >= m_length)
		{
			toplevel()->throwRangeError(kStringIndexOutOfBoundsError, core()->toErrorString(index), core()->toErrorString(0), core()->toErrorString(m_length));
		}
		return new (gc()) String(m_buffer+index, 1);
	}

	uint32 StringBuilderObject::charCodeAt(uint32 index)
	{
		if (index > m_length)
		{
			toplevel()->throwRangeError(kStringIndexOutOfBoundsError, core()->toErrorString(index), core()->toErrorString(0), core()->toErrorString(m_length));
		}
		return m_buffer[index];
	}

	void StringBuilderObject::ensureCapacity(uint32 minimumCapacity)
	{
		if (m_capacity < minimumCapacity)
		{
			/* The new capacity is the larger of
			 * - The minimumCapacity argument
			 * - Twice the old capacity, plus 2
			 */
			uint32 newCapacity = (m_capacity<<1)+2;
			if (newCapacity < minimumCapacity)
			{
				newCapacity = minimumCapacity;
			}

			wchar *newBuffer = new wchar[newCapacity];
			if (newBuffer == NULL)
			{
				toplevel()->throwError(kOutOfMemoryError);
			}
			
			memcpy(newBuffer, m_buffer, m_length*sizeof(wchar));
			delete [] m_buffer;
			m_buffer = newBuffer;
			m_capacity = newCapacity;
		}
	}

	int StringBuilderObject::indexOf(Stringp str, uint32 index)
	{
		if (!str) {
			toplevel()->throwArgumentError(kNullArgumentError, "str");
		}
		
		uint32 sublen = str->length();
		if (m_length < sublen)
		{
			return -1;
		}

		// endIndex is the last character in selfString subStr could be found at
		// (and further, and there isn't enough of selfString remaining for a match to be possible)
		const uint32 endIndex = m_length - sublen; 
		
		const wchar *ptr = m_buffer + index;
		const wchar *subchars = str->c_str();
		for ( ; index <= endIndex; index++)
		{
			if (memcmp(ptr, subchars, sublen*sizeof(wchar)) == 0)
			{
				return index;
			}
			ptr++;
		}

		return -1;
	}

	void StringBuilderObject::insert(uint32 index, Atom value)
	{
		Stringp str = core()->string(value);
		uint32 len = str->length();

		if (index > m_length)
		{
			toplevel()->throwRangeError(kStringIndexOutOfBoundsError, core()->toErrorString(index), core()->toErrorString(0), core()->toErrorString(m_length));
		}
		
		ensureCapacity(m_length + len);
		memmove(m_buffer+index+len, m_buffer+index, (m_length-index)*sizeof(wchar));
		memcpy(m_buffer+index, str->c_str(), len*sizeof(wchar));
		m_length += len;
	}

	int StringBuilderObject::lastIndexOf(Stringp substr, uint32 index)
	{
		if (!substr) {
			toplevel()->throwArgumentError(kNullArgumentError, "str");
		}
		
		uint32 sublen = substr->length();

		// endIndex is the last character in selfString subStr could be found at
		// (and further, and there isn't enough of selfString remaining for a match to be possible)
		const uint32 endIndex = m_length - sublen;

		if (index == 0xFFFFFFFF)
		{
			index = endIndex;
		}
		if (index > m_length)
		{
			toplevel()->throwRangeError(kStringIndexOutOfBoundsError, core()->toErrorString(index), core()->toErrorString(0), core()->toErrorString(m_length));
		}
		if (index > endIndex)
		{
			index = endIndex;
		}
		
		const wchar *ptr = m_buffer + index;
		for ( ; index >= 0 ; index-- )
		{
			if (memcmp(ptr, substr->c_str(), sublen*sizeof(wchar)) == 0) {
				return index;
			}
			ptr--;
		}

		return -1;
	}

	uint32 StringBuilderObject::get_length()
	{
		return m_length;
	}

	void StringBuilderObject::set_length(uint32 length)
	{
		ensureCapacity(length);
		if (length > m_length)
		{
			memset(m_buffer+m_length, 0, (length-m_length)*sizeof(wchar));
		}
		m_length = length;
	}

	void StringBuilderObject::remove(uint32 start, uint32 end)
	{
		if (start > end)
		{
			toplevel()->throwRangeError(kInvalidRangeError);
		}
		if (start >= m_length)
		{
			toplevel()->throwRangeError(kStringIndexOutOfBoundsError, core()->toErrorString(start), core()->toErrorString(0), core()->toErrorString(m_length));
		}
		if (end > m_length)
		{
			toplevel()->throwRangeError(kStringIndexOutOfBoundsError, core()->toErrorString(end), core()->toErrorString(0), core()->toErrorString(m_length));
		}
		if (start == end)
		{
			return;
		}
		memcpy(m_buffer+start, m_buffer+end, (m_length-end)*sizeof(wchar));
		m_length -= (end - start);
	}

	void StringBuilderObject::removeCharAt(uint32 index)
	{
		if (index >= m_length)
		{
			toplevel()->throwRangeError(kStringIndexOutOfBoundsError, core()->toErrorString(index), core()->toErrorString(0), core()->toErrorString(m_length));
		}
		memcpy(m_buffer+index, m_buffer+index+1, (m_length-index-1)*sizeof(wchar));
		m_length--;
	}

	void StringBuilderObject::replace(uint32 start, uint32 end, Stringp replacement)
	{
		if (!replacement) {
			toplevel()->throwArgumentError(kNullArgumentError, "replacement");
		}
		
		if (start > end)
		{
			toplevel()->throwRangeError(kInvalidRangeError);
		}
		if (start >= m_length)
		{
			toplevel()->throwRangeError(kStringIndexOutOfBoundsError, core()->toErrorString(start), core()->toErrorString(0), core()->toErrorString(m_length));
		}
		if (end > m_length)
		{
			toplevel()->throwRangeError(kStringIndexOutOfBoundsError, core()->toErrorString(end), core()->toErrorString(0), core()->toErrorString(m_length));
		}

		uint32 replacementLength = replacement->length();

		uint32 newLength = m_length + replacementLength - (end - start);
		ensureCapacity(newLength);
		
		memmove(m_buffer + start + replacementLength,
				m_buffer + end,
				(m_length - end) * sizeof(wchar));
		memcpy(m_buffer + start,
			   replacement->c_str(),
			   replacementLength * sizeof(wchar));
		m_length = newLength;
	}
		
	void StringBuilderObject::reverse()
	{
		wchar *start = m_buffer;
		wchar *end   = m_buffer+m_length;

		if (m_length)
		{
			end--;
		}

		while (start < end)
		{
			wchar ch = *start;
			*start = *end;
			*end = ch;
			start++;
			end--;
		}
	}

	void StringBuilderObject::setCharAt(uint32 index, Stringp ch)
	{
		if (!ch || !ch->length()) {
			toplevel()->throwArgumentError(kNullArgumentError, "ch");
		}
		
		ensureCapacity(index+1);
		if (index >= m_length)
		{
			set_length(index+1);
		}
		m_buffer[index] = ch->c_str()[0];
	}

	Stringp StringBuilderObject::substring(uint32 start, uint32 end)
	{
		if (start > end)
		{
			toplevel()->throwRangeError(kInvalidRangeError);
		}
		if (start >= m_length)
		{
			toplevel()->throwRangeError(kStringIndexOutOfBoundsError, core()->toErrorString(start), core()->toErrorString(0), core()->toErrorString(m_length));
		}
		if (end > m_length)
		{
			toplevel()->throwRangeError(kStringIndexOutOfBoundsError, core()->toErrorString(end), core()->toErrorString(0), core()->toErrorString(m_length));
		}
		if (start == end)
		{
			return core()->kEmptyString;
		}
		return new (gc()) String(m_buffer+start, end-start);
	}

	Stringp StringBuilderObject::_toString()
	{
		return new (gc()) String(m_buffer, m_length);
	}

	void StringBuilderObject::trimToSize()
	{
		if (m_length == m_capacity)
		{
			// already trimmed
			return;
		}
		uint32 newCapacity = m_length;
		if (newCapacity < kInitialCapacity)
		{
			newCapacity = kInitialCapacity;
		}
		wchar *newBuffer = new wchar[newCapacity];
		if (!newBuffer)
		{
			// can't trim
			return;
		}
		delete [] m_buffer;
		memcpy(newBuffer, m_buffer, m_length*sizeof(wchar));
		m_capacity = newCapacity;
		m_buffer = newBuffer;
	}

	StringBuilderClass::StringBuilderClass(VTable *cvtable)
		: ClassClosure(cvtable)
	{
		createVanillaPrototype();
	}

	ScriptObject* StringBuilderClass::createInstance(VTable *ivtable,
													 ScriptObject *prototype)
	{
		return new (core()->GetGC(), ivtable->getExtraSize()) StringBuilderObject(ivtable, prototype);
	}
}
