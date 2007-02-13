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


#ifndef BYTEARRAYGLUE_INCLUDED
#define BYTEARRAYGLUE_INCLUDED

namespace avmshell
{
	class ByteArray
	{
	public:
		ByteArray();
		ByteArray(const ByteArray &lhs);
		~ByteArray();

		U8 operator [] (uint32 index) const;
		U8& operator [] (uint32 index);
		void Push(U8 value);
		void Push(const U8 *buffer, uint32 count);
		uint32 GetLength() const { return m_length; }
		void SetLength(uint32 newLength);
		bool Grow(uint32 newCapacity);
		U8 *GetBuffer() const { return m_array; }

	protected:
		void ThrowMemoryError();

		uint32 m_capacity;
		uint32 m_length;
		U8 *m_array;

		enum { kGrowthIncr = 4096 };
	};

	class ByteArrayFile : public ByteArray, public DataInput, public DataOutput
	{
	public:
		ByteArrayFile(Toplevel *toplevel);
		virtual ~ByteArrayFile()
		{
			m_filePointer = 0; 
		}

		uint32 GetFilePointer() { return m_filePointer; }
		void Seek(uint32 filePointer) { m_filePointer = filePointer; }
		void SetLength(uint32 newLength);
		uint32 Available();
		void Read(void *buffer, uint32 count);
		void Write(const void *buffer, uint32 count);
		
	private:
		uint32 m_filePointer;
	};
	
	class ByteArrayObject : public ScriptObject
	{
	public:
		ByteArrayObject(VTable *ivtable, ScriptObject *delegate);

		void fill(const void *b, int len);
	
		void checkNull(void *instance, const char *name);

		uint32 getLength() { return m_byteArray.GetLength(); }
		void setLength(uint32 newLength);

		virtual bool hasAtomProperty(Atom name) const;
		virtual void setAtomProperty(Atom name, Atom value);
		virtual Atom getAtomProperty(Atom name) const;
		virtual Atom getUintProperty(uint32 i) const;
		virtual void setUintProperty(uint32 i, Atom value);

		void readBytes(ByteArrayObject *bytes, uint32 offset, uint32 length);
		void writeBytes(ByteArrayObject *bytes, uint32 offset, uint32 length);

		String* _toString();

		// renamed to avoid preprocessor conflict with mozilla's zlib, which #define's compress and uncompress
		void zlib_compress();
		void zlib_uncompress();

		void writeBoolean(bool value);
		void writeByte(int value);
		void writeShort(int value);
		void writeInt(int value);
		void writeUnsignedInt(uint32 value);
		void writeFloat(double value);
		void writeDouble(double value);
		void writeUTF(String *value);
		void writeUTFBytes(String *value);
	
		bool readBoolean();
		int readByte();
		int readUnsignedByte();
		int readShort();
		int readUnsignedShort();
		int readInt();
		uint32 readUnsignedInt();
		double readFloat();
		double readDouble();
		String* readUTF();
		String* readUTFBytes(uint32 length);		
		
		int available();
		int getFilePointer();
		void seek(int offset);

		uint32 get_length();
		void set_length(uint32 value);

		Stringp get_endian();
		void set_endian(Stringp type);
		
		ByteArray& GetByteArray() { return m_byteArray; }

		void writeFile(Stringp filename);

	private:
		MMgc::Cleaner c;
		ByteArrayFile m_byteArray;
	};

	//
	// ByteArrayClass
	//
	
	class ByteArrayClass : public ClassClosure
	{
    public:
		ByteArrayClass(VTable *vtable);

		ScriptObject *createInstance(VTable *ivtable, ScriptObject *delegate);

		ByteArrayObject *readFile(Stringp filename);

		DECLARE_NATIVE_MAP(ByteArrayClass)
    };
}

#endif /* BYTEARRAYGLUE_INCLUDED */
