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
#include "genericzlib.h"

namespace avmshell
{
	BEGIN_NATIVE_MAP(ByteArrayClass)
		NATIVE_METHOD(flash_utils_ByteArray_toString,          ByteArrayObject::_toString)
		NATIVE_METHOD(flash_utils_ByteArray_length_get,        ByteArrayObject::get_length)
		NATIVE_METHOD(flash_utils_ByteArray_length_set,        ByteArrayObject::set_length)

		NATIVE_METHOD(flash_utils_ByteArray_readBytes,         ByteArrayObject::readBytes)
		NATIVE_METHOD(flash_utils_ByteArray_writeBytes,        ByteArrayObject::writeBytes)

		NATIVE_METHOD(flash_utils_ByteArray_writeBoolean,      ByteArrayObject::writeBoolean)
		NATIVE_METHOD(flash_utils_ByteArray_writeByte,         ByteArrayObject::writeByte)
		NATIVE_METHOD(flash_utils_ByteArray_writeShort,        ByteArrayObject::writeShort)
		NATIVE_METHOD(flash_utils_ByteArray_writeInt,          ByteArrayObject::writeInt)
		NATIVE_METHOD(flash_utils_ByteArray_writeUnsignedInt,  ByteArrayObject::writeUnsignedInt)		
		NATIVE_METHOD(flash_utils_ByteArray_writeFloat,        ByteArrayObject::writeFloat)
		NATIVE_METHOD(flash_utils_ByteArray_writeDouble,       ByteArrayObject::writeDouble)
		NATIVE_METHOD(flash_utils_ByteArray_writeUTF,          ByteArrayObject::writeUTF)
		NATIVE_METHOD(flash_utils_ByteArray_writeUTFBytes,     ByteArrayObject::writeUTFBytes)		
	
		NATIVE_METHOD(flash_utils_ByteArray_readBoolean,       ByteArrayObject::readBoolean)
		NATIVE_METHOD(flash_utils_ByteArray_readByte,          ByteArrayObject::readByte)
		NATIVE_METHOD(flash_utils_ByteArray_readUnsignedByte,  ByteArrayObject::readUnsignedByte)
		NATIVE_METHOD(flash_utils_ByteArray_readShort,         ByteArrayObject::readShort)
		NATIVE_METHOD(flash_utils_ByteArray_readUnsignedShort, ByteArrayObject::readUnsignedShort)
		NATIVE_METHOD(flash_utils_ByteArray_readInt,           ByteArrayObject::readInt)
		NATIVE_METHOD(flash_utils_ByteArray_readUnsignedInt,   ByteArrayObject::readUnsignedInt)		
		NATIVE_METHOD(flash_utils_ByteArray_readFloat,         ByteArrayObject::readFloat)
		NATIVE_METHOD(flash_utils_ByteArray_readDouble,        ByteArrayObject::readDouble)
		NATIVE_METHOD(flash_utils_ByteArray_readUTF,           ByteArrayObject::readUTF)
		NATIVE_METHOD(flash_utils_ByteArray_readUTFBytes,      ByteArrayObject::readUTFBytes)		
		
		NATIVE_METHOD(flash_utils_ByteArray_bytesAvailable_get,         ByteArrayObject::available)
		NATIVE_METHOD(flash_utils_ByteArray_position_get,    ByteArrayObject::getFilePointer)
		NATIVE_METHOD(flash_utils_ByteArray_position_set,              ByteArrayObject::seek)
		NATIVE_METHOD(flash_utils_ByteArray_compress,          ByteArrayObject::zlib_compress)
		NATIVE_METHOD(flash_utils_ByteArray_uncompress,        ByteArrayObject::zlib_uncompress)
		NATIVE_METHOD(flash_utils_ByteArray_endian_get,    ByteArrayObject::get_endian)
		NATIVE_METHOD(flash_utils_ByteArray_endian_set,              ByteArrayObject::set_endian)
		NATIVE_METHOD(flash_utils_ByteArray_writeFile,	ByteArrayObject::writeFile)

		NATIVE_METHOD(flash_utils_ByteArray_readFile, ByteArrayClass::readFile)
	END_NATIVE_MAP()
		
	//
	// ByteArray
	//
	
	ByteArray::ByteArray()
	{
		m_capacity = 0;
		m_length   = 0;
		m_array    = NULL;
	}

	ByteArray::ByteArray(const ByteArray &lhs)
	{
		m_array    = new U8[lhs.m_length];
		if (!m_array)
		{
			ThrowMemoryError();
			return;
		}

		m_capacity = lhs.m_length;
		m_length   = lhs.m_length;

		memcpy(m_array, lhs.m_array, m_length);
	}

	ByteArray::~ByteArray()
	{
		if (m_array)
		{
			delete [] m_array;
			m_array = NULL;
		}
		m_capacity = 0;
		m_length = 0;
	}

	void ByteArray::ThrowMemoryError()
	{
		// todo throw out of memory exception
		// m_toplevel->memoryError->throwError(kOutOfMemoryError);
	}

	bool ByteArray::Grow(uint32 minimumCapacity)
	{
		if (minimumCapacity > m_capacity)
		{
			uint32 newCapacity = m_capacity << 1;			
			if (newCapacity < minimumCapacity)
			{
				newCapacity = minimumCapacity;
			}
			if (newCapacity < kGrowthIncr) 
			{
				newCapacity = kGrowthIncr;
			}
			U8 *newArray = new U8[newCapacity];
			if (!newArray)
			{
				return false;
			}
			if (m_array)
			{
				memcpy(newArray, m_array, m_length);
				delete [] m_array;
			}
			memset(newArray+m_length, 0, newCapacity-m_capacity);
			m_array = newArray;
			m_capacity = newCapacity;
		}
		return true;
	}
		
	U8 ByteArray::operator[] (uint32 index) const
	{
		if (m_length <= index)
		{
			return 0;
		}
		return m_array[index];
	}

	U8& ByteArray::operator[] (uint32 index)
	{
		if (m_length <= index)
		{
			Grow(index+1);
			m_length = index+1;
		}
		return m_array[index];
	}

	void ByteArray::Push(U8 value)
	{
		if (m_length >= m_capacity)
		{
			Grow(m_length + 1);
		}
		m_array[m_length++] = value;
	}
		
	void ByteArray::Push(const U8 *data, uint32 count)
	{
		Grow(m_length + count);
		memcpy(m_array + m_length, data, count);
		m_length += count;
	}
	
	void ByteArray::SetLength(uint32 newLength)
	{
		if (newLength > m_capacity)
		{
			if (!Grow(newLength))
			{
				ThrowMemoryError();
				return;
			}
		}
		m_length = newLength;
	}

	//
	// ByteArrayFile
	//

	ByteArrayFile::ByteArrayFile(Toplevel *toplevel)
		: DataInput(toplevel),
		  DataOutput(toplevel)
	{
		m_filePointer = 0;
	}

	uint32 ByteArrayFile::Available()
	{
		if (m_filePointer <= m_length) {
			return m_length - m_filePointer;
		} else {
			return 0;
		}
	}

	void ByteArrayFile::SetLength(uint32 newLength)
	{
		ByteArray::SetLength(newLength);
		if (m_filePointer > newLength) {
			m_filePointer = newLength;
		}
	}
	
	void ByteArrayFile::Read(void *buffer, uint32 count)
	{
		CheckEOF(count);

		if (count > 0)
		{
			memcpy(buffer, m_array+m_filePointer, count);
			m_filePointer += count;
		}
	}

	void ByteArrayFile::Write(const void *buffer, uint32 count)
	{
		if (m_filePointer+count >= m_length) {
			Grow(m_filePointer+count);
			m_length = m_filePointer+count;
		}
		memcpy(m_array+m_filePointer, buffer, count);
		m_filePointer += count;
	}

	//
	// ByteArrayObject
	//
	
	ByteArrayObject::ByteArrayObject(VTable *ivtable,
									 ScriptObject *delegate)
		: ScriptObject(ivtable, delegate),
		  m_byteArray(toplevel())
	{
		c.set(&m_byteArray, sizeof(ByteArrayFile));
	}

	Atom ByteArrayObject::getUintProperty(uint32 i) const
	{
		if (i < (uint32)m_byteArray.GetLength()) {
			return core()->intToAtom(m_byteArray[i]);
		} else {
			return undefinedAtom;
		}
	}
	
	void ByteArrayObject::setUintProperty(uint32 i, Atom value)
	{
		m_byteArray[i] = core()->integer(value);
	}
	
	Atom ByteArrayObject::getProperty(Atom name) const
	{
		AvmCore *core = this->core();
		uint32 index;
		if (core->getIndexFromAtom(name, &index)) {
			if (index < (uint32) m_byteArray.GetLength()) {
				return core->intToAtom(m_byteArray[index]);
			} else {
				return undefinedAtom;
			}
		}

		return ScriptObject::getProperty(name);
	}
	
	void ByteArrayObject::setProperty(Atom name, Atom value)
	{
		AvmCore *core = this->core();
		uint32 index;
		if (core->getIndexFromAtom(name, &index)) {
			int intValue = core->integer(value);
			m_byteArray[index] = intValue;
		} else {
			ScriptObject::setProperty(name, value);
		}
	}
	
	bool ByteArrayObject::hasProperty(Atom name) const
	{
		return ScriptObject::hasProperty(name) || getProperty(name) != undefinedAtom;
	}

	void ByteArrayObject::setLength(uint32 newLength)
	{
		m_byteArray.SetLength(newLength);
	}

	uint32 ByteArrayObject::get_length()
	{
		return m_byteArray.GetLength();
	}

	void ByteArrayObject::set_length(uint32 value)
	{
		setLength(value);
	}

	int ByteArrayObject::getFilePointer()
	{
		return m_byteArray.GetFilePointer();
	}

	int ByteArrayObject::available()
	{
		return m_byteArray.Available();
	}
	
	void ByteArrayObject::seek(int offset)
	{
		if (offset >= 0) {
			m_byteArray.Seek(offset);
		}
	}

	String* ByteArrayObject::_toString()
	{
		unsigned char *c = (unsigned char*)m_byteArray.GetBuffer();
		uint32 len = m_byteArray.GetLength();

		if (len >= 3)
		{
			// UTF8 BOM
			if ((c[0] == 0xef) && (c[1] == 0xbb) && (c[2] == 0xbf))
			{
				return core()->newString(((char *)c) + 3, len - 3);
			}
			else if ((c[0] == 0xfe) && (c[1] == 0xff))
			{
				//UTF-16 big endian
				c += 2;
				len = (len - 2) >> 1;
				Stringp out = new (core()->GetGC()) String(len);
				wchar *buffer = out->lockBuffer();
				for (uint32 i = 0; i < len; i++)
				{
					buffer[i] = (c[0] << 8) + c[1];
					c += 2;
				}
				out->unlockBuffer();

				return out;
			}
			else if ((c[0] == 0xff) && (c[1] == 0xfe))
			{
				//UTF-16 little endian
				c += 2;
				len = (len - 2) >> 1;
				Stringp out = new (core()->GetGC()) String(len);
				wchar *buffer = out->lockBuffer();
				for (uint32 i = 0; i < len; i++)
				{
					buffer[i] = (c[1] << 8) + c[0];
					c += 2;
				}
				out->unlockBuffer();
				return out;
			}
		}

		return core()->newString(((char *)c), len);
	}
	
	int ByteArrayObject::readByte()
	{
		return (signed char)m_byteArray.ReadU8();
	}

	int ByteArrayObject::readUnsignedByte()
	{
		return m_byteArray.ReadU8();
	}

	int ByteArrayObject::readShort()
	{
		return (short)m_byteArray.ReadU16();
	}

	int ByteArrayObject::readUnsignedShort()
	{
		return m_byteArray.ReadU16();
	}

	int ByteArrayObject::readInt()
	{
		return (int)m_byteArray.ReadU32();		
	}

	uint32 ByteArrayObject::readUnsignedInt()
	{
		return m_byteArray.ReadU32();		
	}
	
	double ByteArrayObject::readFloat()
	{
		return m_byteArray.ReadFloat();
	}

	double ByteArrayObject::readDouble()
	{
		return m_byteArray.ReadDouble();
	}

	bool ByteArrayObject::readBoolean()
	{
		return m_byteArray.ReadBoolean();
	}

	void ByteArrayObject::writeBoolean(bool value)
	{
		m_byteArray.WriteBoolean(value);
	}

	void ByteArrayObject::writeByte(int value)
	{
		m_byteArray.WriteU8((U8)value);
	}

	void ByteArrayObject::writeShort(int value)
	{
		m_byteArray.WriteU16((unsigned short)value);
	}

	void ByteArrayObject::writeInt(int value)
	{
		m_byteArray.WriteU32((uint32)value);
	}

	void ByteArrayObject::writeUnsignedInt(uint32 value)
	{
		m_byteArray.WriteU32(value);
	}
	
	void ByteArrayObject::writeFloat(double value)
	{
		m_byteArray.WriteFloat((float)value);
	}

	void ByteArrayObject::writeDouble(double value)
	{
		m_byteArray.WriteDouble(value);
	}

	void ByteArrayObject::zlib_compress()
	{
		int len = m_byteArray.GetLength();
		if (!len) // empty buffer should give us a empty result
			return; 
		unsigned long gzlen = len * 3/2 + 32; // enough for growth plus zlib headers
		U8 *gzdata = new U8[gzlen];

		// Use zlib to compress the data
		compress2((U8*)gzdata, (unsigned long*)&gzlen,
				m_byteArray.GetBuffer(), len, 9);

		// Replace the byte array with the compressed data
		m_byteArray.SetLength(0);
		//m_byteArray.WriteU32((U32)len);
		m_byteArray.Write(gzdata, gzlen);

		delete [] gzdata;
	}

    void ByteArrayObject::zlib_uncompress()
    {
        // Snapshot the compressed data.
        unsigned long gzlen = m_byteArray.GetLength();
		if (!gzlen) // empty buffer should give us a empty result
			return; 

		U8 *gzdata = new U8[gzlen];
        memcpy(gzdata, m_byteArray.GetBuffer(), gzlen);

        // Clear the buffer
        m_byteArray.Seek(0);
        m_byteArray.SetLength(0);

        // The following block is to force destruction
        // of zstream before potential exception throw.
        int error = Z_OK;
        {
            // Decompress the data
            PlatformZlibStream zstream;
            zstream.SetNextIn(gzdata);
            zstream.SetAvailIn(gzlen);

            const int kBufferSize = 8192;
            U8 *buffer = new U8 [kBufferSize];

            do {
                zstream.SetNextOut(buffer);
                zstream.SetAvailOut(kBufferSize);
                error = zstream.InflateWithStatus();
                m_byteArray.Write(buffer, kBufferSize-zstream.AvailOut());
            } while (error == Z_OK);

            delete [] buffer;
            delete [] gzdata;
        }

		// position byte array at the beginning
		m_byteArray.Seek(0);

        if (error != Z_OK && error != Z_STREAM_END) {
            toplevel()->throwError(kShellCompressedDataError);
        }
    }

	void ByteArrayObject::checkNull(void *instance, const char *name)
	{
		if (instance == NULL) {
			toplevel()->throwTypeError(kNullArgumentError, core()->toErrorString(name));
		}
	}	
	void ByteArrayObject::writeBytes(ByteArrayObject *bytes,
									 uint32 offset,
									 uint32 length)
	{
		checkNull(bytes, "bytes");

		if (length == 0) {
			length = bytes->getLength() - offset;
		}
		
		m_byteArray.WriteByteArray(bytes->GetByteArray(),
								   offset,
								   length);
	}

	void ByteArrayObject::readBytes(ByteArrayObject *bytes,
									uint32 offset,
									uint32 length)
	{
		checkNull(bytes, "bytes");

		if (length == 0) {
			length = m_byteArray.Available();
		}
		
		m_byteArray.ReadByteArray(bytes->GetByteArray(),
								  offset,
								  length);
	}
	
	String* ByteArrayObject::readUTF()
	{
		return m_byteArray.ReadUTF();
	}

	String* ByteArrayObject::readUTFBytes(uint32 length)
	{
		return m_byteArray.ReadUTFBytes(length);
	}
	
	void ByteArrayObject::writeUTF(String *value)
	{
		checkNull(value, "value");
		m_byteArray.WriteUTF(value);
	}

	void ByteArrayObject::writeUTFBytes(String *value)
	{
		checkNull(value, "value");
		m_byteArray.WriteUTFBytes(value);
	}

	void ByteArrayObject::fill(const void *b, int len)
	{
		m_byteArray.Write(b, len);
	}
	
	//
	// ByteArrayClass
	//

	ByteArrayClass::ByteArrayClass(VTable *vtable)
		: ClassClosure(vtable)
    {
        prototype = toplevel()->objectClass->construct();
	}

	ScriptObject* ByteArrayClass::createInstance(VTable *ivtable,
												 ScriptObject *prototype)
    {
        return new (core()->GetGC(), ivtable->getExtraSize()) ByteArrayObject(ivtable, prototype);
    }


	ByteArrayObject * ByteArrayClass::readFile(Stringp filename)
	{
		Toplevel* toplevel = this->toplevel();
		if (!filename) {
			toplevel->throwArgumentError(kNullArgumentError, "filename");
		}
		UTF8String* filenameUTF8 = filename->toUTF8String();
		FILE *fp = fopen(filenameUTF8->c_str(), "rb");
		if (fp == NULL) {
			toplevel->throwError(kFileOpenError, filename);
		}
		fseek(fp, 0L, SEEK_END);
		long len = ftell(fp);
		rewind(fp);

		unsigned char *c = new unsigned char[len+1];

		Atom args[1] = {nullObjectAtom};
		ByteArrayObject *b = (ByteArrayObject*)AvmCore::atomToScriptObject(construct(0,args));
		b->setLength(0);

		while (len > 0)
		{
			int actual = fread(c, 1, len, fp);
			if (actual > 0)
			{
				b->fill(c, actual);
				len -= actual;
			}
			else
			{
				break;
			}
		}
		b->seek(0);

		delete [] c;
		return b;
	}

	Stringp ByteArrayObject::get_endian()
	{
		return (m_byteArray.GetEndian() == kBigEndian) ? core()->constantString("bigEndian") : core()->constantString("littleEndian");
	}

	void ByteArrayObject::set_endian(Stringp type)
	{
		AvmCore* core = this->core();
		type = core->internString(type);
		if (type == core->constantString("bigEndian"))
		{
			m_byteArray.SetEndian(kBigEndian);
		}
		else if (type == core->constantString("littleEndian"))
		{
			m_byteArray.SetEndian(kLittleEndian);
		}
		else
		{
			toplevel()->throwArgumentError(kInvalidArgumentError, "type");
		}
	}

	void ByteArrayObject::writeFile(Stringp filename)
	{
		Toplevel* toplevel = this->toplevel();
		if (!filename) {
			toplevel->throwArgumentError(kNullArgumentError, "filename");
		}

		UTF8String* filenameUTF8 = filename->toUTF8String();
		FILE *fp = fopen(filenameUTF8->c_str(), "wb");
		if (fp == NULL) {
			toplevel->throwError(kFileWriteError, filename);
		}

		fwrite(&(this->GetByteArray())[0], this->get_length(), 1, fp);
		fclose(fp);
	}

}	


