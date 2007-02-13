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

#ifndef __avmplus_String__
#define __avmplus_String__


namespace avmplus
{
	/**
	 * A string in UTF-8 encoding.
	 *
	 * UTF8String's are immutable and garbage collected, which makes
	 * it easy to pass them around.
	 */
	class UTF8String : public MMgc::GCObject
	{
	public:
		/**
		 * Constructs a UTF8String.  This should not be called
		 * directly; use the toUTF8String method of String.
		 */
		UTF8String(int _length) { m_length = _length; }

		/**
		 * Operator overload; returns a pointer to the
		 * null-terminated string.
		 */
		operator const char* () const { return m_buffer; }

		/**
		 * Returns a pointer to the null-terminated string.
		 */
		const char *c_str() const { return m_buffer; }

		/**
		 * Returns the length of the string in bytes,
		 * excluding the null terminator.
		 */
		int length() const { return m_length; }

		/**
		 * This is an advanced method which returns a non-const
		 * pointer to the UTF8String's internal buffer.  This
		 * can be used to mutate a string that is known to not
		 * have any other references.  Use with caution.
		 */
		char *lockBuffer() { return m_buffer; }

		/**
		 * Unlocks the buffer previously returned by lockBuffer.
		 * Currently a no-op, but may change in the future,
		 * so call after using lockBuffer.
		 */
		void unlockBuffer() {}
		
	private:
		int m_length;
		char m_buffer[1];
	};

	/**
	 * A string in UTF-16 encoding.  This is the basic string
	 * class used by AVM+ code.
	 */
	class String : public AvmPlusScriptableObject
	{
	public:
		String(const wchar *str, int len);					// wchar[] -> string
		String(const char *str, int utf8len, int utf16len); // utf8->string
		String(Stringp s1, Stringp s0);		// concat
		String(Stringp s, int pos, int len);// substr
		String(int len);					// preallocated empty

		~String()
		{
#ifdef MMGC_DRC
			setBuf(NULL);
			setPrefixOrOffsetOrNumber(0);
			m_length = 0;
#endif
		}

		/**
		 * Converts this string to a UTF-8 string.  Allocates
		 * a new UTF8 string object containing the result,
		 * and returns it.
		 */
		UTF8String* toUTF8String();

		/**
		 * Returns the Atom equivalent of this String.  This is
		 * done by or'ing the proper type bits into the pointer.
		 */
		Atom atom() const { return AtomConstants::kStringType | (Atom)this; }

		/**
		 * virtual version of atom():
		 */
		virtual Atom toAtom() const { return atom(); }

		/**
		 * Returns the length of the string in characters.
		 * The null terminator is not included.
		 */
		int length() const { 
			return m_length & 0x7FFFFFFF; 
		}

		/**
		 * Operator overload; returns a pointer to the
		 * null-terminated string.
		 */
		operator const wchar* () 
		{
			// For offset too since our string needs to be null terminated
			if (needsNormalization()) normalize();
			return getData(); 
		}
		
		/**
		 * Returns a pointer to the null-terminated string.
		 */
		const wchar* c_str() 
		{
			// For offset too since our string needs to be null terminated
			if (needsNormalization()) normalize();
			return getData(); 
		}

		/**
		 * Returns the index'th character of the string.
		 * @param index zero-based index into the string
		 */
		wchar operator[] (int index);

		/**
		 * This is an advanced method which returns a non-const
		 * pointer to the String's internal buffer.  This
		 * can be used to mutate a string that is known to not
		 * have any other references.  Use with caution.
		 */
		wchar* lockBuffer() 
		{
			// For offset too since our string needs to be null terminated
			if (needsNormalization()) normalize();
			return (wchar*) getData(); 
		}
		
		/**
		 * Unlocks the buffer previously returned by lockBuffer.
		 * Must call after using lockBuffer to mutate the buffer.
		 */
		void unlockBuffer(int newLen) 
		{
			AvmAssert(!isInterned());
			m_length = newLen;
		}
		void unlockBuffer() {}

		/**
		 * Returns a new string object which is a copy of this
		 * string object, with all characters in the string
		 * converted to uppercase.
		 *
		 * Unicode character classes for uppercase and lowercase
		 * are used.  The conversion behavior is compliant with
		 * the String.toUpperCase method.
		 */
		Stringp toUpperCase();

		/**
		 * Returns a new string object which is a copy of this
		 * string object, with all characters in the string
		 * converted to lowercase.
		 *
		 * Unicode character classes for uppercase and lowercase
		 * are used.  The conversion behavior is compliant with
		 * the String.toLowerCase method.
		 */
		Stringp toLowerCase();

		/*@{*/
		/**
		 * Compare the String with toCompare.
		 * @return = 0 if the strings are identical.
		 *         < 0 if this string is less than toCompare
		 *         > 0 if this string is greater than toCompare
		 */
		int Compare(String& toCompare) 
		{
			if (hasPrefix()) normalize();
			if (toCompare.hasPrefix()) toCompare.normalize();

			return String::Compare(getData() + getOffset(), length(), toCompare.getData() + toCompare.getOffset(), toCompare.length());
		}
		
		/*@{*/
		/**
		 * Does String contain wchar?
		 * @return = 0 if the strings are identical.
		 *         < 0 if this string is less than toCompare
		 *         > 0 if this string is greater than toCompare
		 */
		bool Contains(wchar c);
		
		// compare this string to (other,len)
		bool Equals(const wchar *toCompare, int len)
		{
			AvmAssert(toCompare[len]==0);
			int sLen = length();
			if (len != sLen) return false;
			if (hasPrefix()) normalize();
			return String::Compare(getData() + getOffset(), sLen, toCompare, len)==0;
		}

		// toCompare is not necessarily zero-terminated at toCompare[len]
		bool FastEquals(const wchar *toCompare, int len)
		{
			int sLen = length();
			if (len != sLen) return false;
			// This is only for intern strings which are never offset or prefix
			AvmAssert(needsNormalization() == false);
			const wchar *src = getData();

			// !! could we compare two WORDS at a time?  Our toCompare
			// string is not necessarily DWORD aligned.  (Offset strings, etc.)

			while (sLen)
			{
				sLen--;
				if (src[sLen] != toCompare[sLen])
					return false;
			}
				
			AvmAssert(sLen == 0);
			return true;
		}

		// compare this string to null-terminated 8bit string
		bool Equals(const char *other8)
		{ 
			if (hasPrefix()) normalize();
			return !Compare(getData() + getOffset(), other8, length());
		}

		/*@{*/
		/**
		 * Compares s1 and s2.
		 * @return = 0 if the strings are identical.
		 *         < 0 if s1 is less than s2
		 *         > 0 if s1 is greater than s2
		 */		
		static int Compare(const wchar *s1,
						   int len1,
						   const wchar *s2,
						   int len2);
		static int Compare(const wchar *s1,
						   const char  *s2,
						   int len);
		/*@}*/

		/*@{*/
		/**
		 * Returns the length of str, in # of characters.
		 */
		static int Length(const wchar *str);
		static int Length(const char *str);
		/*@}*/

		void setInterned(AvmCore *core)
		{
			m_length |= 0x80000000;
			generateIntegerEquivalent (core);
		}

		static bool isSpace(wchar ch)
		{
			return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
		}

		bool isWhitespace();

		int isInterned() const
		{
			return m_length & 0x80000000;
		}

		// handles hex, octal, base 10 integer, float, and "Infinity"/"-Infinity"
		double toNumber() 
		{
			// For offset too since convertStringToNumber expects a null terminated string
			if (needsNormalization()) normalize();
			return MathUtils::convertStringToNumber(getData() + getOffset(), length());
		}

		// native functions
		int indexOf(Stringp s, int i=0);
		int indexOfDouble(Stringp s, double i=0);
		int lastIndexOf(Stringp s, int i=0x7fffffff);
		int lastIndexOfDouble(Stringp s, double i=0x7fffffff);
		Stringp charAt(int i=0); 
		Stringp charAtDouble(double i=0); 
		double charCodeAt(int i); // returns NaN for out-of-bounds
		double charCodeAtDouble(double i); // returns NaN for out-of-bounds
		int localeCompare(Stringp other, Atom *argv, int argc);

		Stringp substring(int i_start, int i_end);
		Stringp substringDouble(double d_start, double d_end);

		Stringp slice(int dStart, int dEnd);
		Stringp sliceDouble(double dStart, double dEnd);

		Stringp substr(int dStart, int dEnd);
		Stringp substrDouble(double dStart, double dEnd);

		// Useful utilities used by the core code.
		static wchar wCharToUpper (wchar ch);
		static wchar wCharToLower (wchar ch);
		
#ifdef DEBUGGER
		uint32 size() const;
#endif

	private:
		int			m_length; // { interned: 1, length:31 }
		class StringBuf : public MMgc::RCObject
		{
		public:
			wchar m_buf[1];
#ifdef MMGC_DRC
			~StringBuf()
			{
				memset(m_buf, 0, MMgc::GC::Size(this)-sizeof(MMgc::RCObject));
			}
#endif
		};
		// no WB b/c manual WB is in setBuf, faster that way
		StringBuf* m_buf;

		// The low two bits control what type of value is stored in m_prefixOrOffsetOrNumber
		// 0x00 nothing is stored (rest of value is 0)
		// 0x01 the 29-bit numeric equivalent of this string is stored (same as kIntegerAtom format)
		// 0x02 a prefix string is stored
		// 0x03 a 30-bit offset is stored
		// manual WB when needed
		uintptr	m_prefixOrOffsetOrNumber;
		#define		STRINGFLAGS		0x03
		#define		NUMBERFLAG		0x01
		#define		PREFIXFLAG		0x02
		#define		OFFSETFLAG		0x03

		Stringp	getPrefix() const 
		{ 
			if ((m_prefixOrOffsetOrNumber & STRINGFLAGS) == PREFIXFLAG) 
				return Stringp(m_prefixOrOffsetOrNumber & ~STRINGFLAGS); 
			else 
				return 0; 
		};

		uint32 getOffset() const 
		{ 
			if ((m_prefixOrOffsetOrNumber & STRINGFLAGS) == OFFSETFLAG) 
				return urshift(m_prefixOrOffsetOrNumber & ~STRINGFLAGS, 2); 
			else 
				return 0; 
		};

		bool hasPrefix() const { return ((m_prefixOrOffsetOrNumber & STRINGFLAGS) == PREFIXFLAG); };
		bool hasOffset() const { return ((m_prefixOrOffsetOrNumber & STRINGFLAGS) == OFFSETFLAG); };

		bool needsNormalization() const { return ((m_prefixOrOffsetOrNumber & STRINGFLAGS) >= 0x2); };

		void normalize();

		// If our string is a valid positive integer that fits in a kIntegerAtom, this
		// will set our m_prefixOrOffsetOrNumber value to the int atom representation or'ed
		// with NUMBERTYPE.  This is only valid for non-prefix, non-offset interned strings.
		void generateIntegerEquivalent(AvmCore *core);

		void setPrefixOrOffsetOrNumber(uintptr value);

		static const wchar lowerCaseBase[];
		static const wchar upperCaseBase[];
		static const wchar lowerCaseConversion[];
		static const wchar upperCaseConversion[];
		static const unsigned char tolower_map[];
		static const unsigned char toupper_map[];
public:
		// This returns a kIntegerAtom Atom
		// for use in our ScriptObject HashTable implementation.  If we have a valid 
		// integer equivalent, it will never be zero since kIntegerType tag != 0
		Atom getIntAtom() const 
		{ 
			if ((m_prefixOrOffsetOrNumber & STRINGFLAGS) == NUMBERFLAG) 
				return m_prefixOrOffsetOrNumber & ~STRINGFLAGS | kIntegerType;
			else 
				return 0; 
		};

		StringBuf* allocBuf(int numChars);
		wchar *getData() const { return m_buf->m_buf; }
		void setBuf(StringBuf *buf) { WBRC(MMgc::GC::GetGC(this), this, &m_buf, buf); }


	};

	// Compare helpers
	inline bool operator==(String& s1, String& s2)
	{ 
		return s1.length() == s2.length() && s1.Compare(s2) == 0;
	}
	inline bool operator!=(String& s1, String& s2)
	{
		return s1.length() != s2.length() || s1.Compare(s2) != 0;
	}
	inline bool operator<(String& s1, String& s2)
	{ 
		return s2.Compare(s1) < 0; 
	}
	inline bool operator>(String& s1, String& s2)
	{ 
		return s2.Compare(s1) > 0;
	}
	inline bool operator<=(String& s1, String& s2)
	{ 
		return s2.Compare(s1) <= 0;
	}
	inline bool operator>=(String& s1, String& s2)
	{ 
		return s2.Compare(s1) >= 0; 
	}
}

#endif /* __avmplus_String__ */
