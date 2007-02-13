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


#include "avmshell.h"

namespace avmshell
{
    #define M_ERROR_CLASS(class_id) ((ErrorClass*)m_toplevel->getPlayerClass(avmplus::NativeID::abcclass_##class_id))
	
	bool DataInput::ReadBoolean()
	{
		U8 value;		

		Read(&value, 1);
		return value != 0;
	}

	U8 DataInput::ReadU8()
	{
		U8 value;

		Read(&value, 1);
		return value;
	}

	unsigned short DataInput::ReadU16()
	{
		unsigned short value;

		Read(&value, 2);
		ConvertU16(value);
		return value;
	}
	
	uint32 DataInput::ReadU32()
	{
		uint32 value;
		Read(&value, 4);
		ConvertU32(value);
		return value;
	}

	float DataInput::ReadFloat()
	{
		union {
			uint32 u;
			float f;
		};
		u = ReadU32();
		return f;
	}

	double DataInput::ReadDouble()
	{
		double value;
		Read(&value, 8);
		ConvertU64((uint64&)value);
		return value;
	}

	String* DataInput::ReadUTFBytes(uint32 length)
	{
		CheckEOF(length);
		
		char *buffer = new char[length+1];
		if (!buffer) {
			ThrowMemoryError();
		}

		Read(buffer, length);
		buffer[length] = 0;
		
		String *out = m_toplevel->core()->newString(buffer);
		delete [] buffer;
		
		return out;
	}
	
	String* DataInput::ReadUTF()
	{
		return ReadUTFBytes(ReadU16());
	}

	void DataInput::ReadByteArray(ByteArray& buffer,
								  uint32 offset,
								  uint32 count)
	{
		uint32 available = Available();
		
		if (count == 0) {
			count = available;
		}
		
		if (count > available) {
			ThrowEOFError();
		}

		// Grow the buffer if necessary
		if (offset + count >= buffer.GetLength()) {
			buffer.SetLength(offset + count);
		}

		Read(buffer.GetBuffer() + offset, count);
	}

	void DataInput::CheckEOF(uint32 count)
	{
		if (Available() < count) {
			ThrowEOFError();
		}
	}

	void DataInput::ThrowEOFError()
	{
		m_toplevel->throwError(kEndOfFileError);
	}

	void DataInput::ThrowMemoryError()
	{
		m_toplevel->throwError(kOutOfMemoryError);
	}
	
	//
	// DataOutput
	//

	void DataOutput::ThrowRangeError()
	{
		m_toplevel->throwRangeError(kInvalidRangeError);
	}
	
	void DataOutput::WriteBoolean(bool value)
	{
		WriteU8(value ? 1 : 0);
	}

	void DataOutput::WriteU8(U8 value)
	{
		Write(&value, 1);
	}

	void DataOutput::WriteU16(unsigned short value)
	{
		ConvertU16(value);
		Write(&value, 2);
	}

	void DataOutput::WriteU32(uint32 value)
	{
		ConvertU32(value);
		Write(&value, 4);
	}
	
	void DataOutput::WriteFloat(float value)
	{
		WriteU32(*((uint32*)&value));
	}

	void DataOutput::WriteDouble(double value)
	{
		ConvertU64((uint64&)value);
		Write(&value, 8);
	}

	void DataOutput::WriteUTF(String *str)
	{
		UTF8String* utf8 = str->toUTF8String();
		uint32 length = utf8->length();
		if (length > 65535) {
			ThrowRangeError();
		}
		WriteU16((unsigned short)length);
		Write(utf8->c_str(), length*sizeof(char));
	}

	void DataOutput::WriteUTFBytes(String *str)
	{
		UTF8String* utf8 = str->toUTF8String();
		int len = utf8->length();
		Write(utf8->c_str(), len*sizeof(char));
	}
	
	void DataOutput::WriteByteArray(ByteArray& buffer,
									uint32 offset,
									uint32 count)
	{
		if (buffer.GetLength() < offset)
			offset = buffer.GetLength();

		if (count == 0) {
			count = buffer.GetLength()-offset;
		}

		if (count > buffer.GetLength()-offset) {
			ThrowRangeError();
		}
			
		if (count > 0) {
			Write(buffer.GetBuffer()+offset, count);
		}
	}
}


