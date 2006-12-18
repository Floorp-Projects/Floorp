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
	int PrintWriter::write(const void *buffer, int count)
	{
		if (m_stream) {
			int result = m_stream->write(buffer, count);

			const char *ptr = (const char *) buffer;
			while (--count >= 0) {
				switch (*ptr) {
				case '\n':
					col = 0;
					break;
				case '\t':
					col = ((col>>3)+1)<<3;
					break;
				default:
					col++;
					break;
				}
				ptr++;
			}

			return result;
		} else {
			return 0;
		}			
	}
	
	PrintWriter& PrintWriter::operator<< (tabstop tabs)
	{
		while (col <= tabs.getSpaces()) {
			*this << ' ';
		}
		return *this;
	}

	PrintWriter& PrintWriter::operator<< (hexAddr value)
	{
		writeHexAddr(value.getValue());
		return *this;
	}
	
	PrintWriter& PrintWriter::operator<< (percent value)
	{
		wchar buffer[256];
		int len;
		MathUtils::convertDoubleToString(value.getPercent(), buffer, len);
		wchar *ptr = buffer;
		while (*ptr) {
			if (*ptr == '.' && *(ptr+1)) {
				*(ptr+2) = 0;
				break;
			}
			ptr++;
		}
		if (value.getPercent() < 10) {
			*this << ' ';
		}
		*this << buffer;
		return *this;
	}
	
	PrintWriter& PrintWriter::operator<< (const char *str)
	{
		write(str, String::Length(str));
		return *this;
	}

	PrintWriter& PrintWriter::operator<< (bool b)
	{
		*this << (b ? "true" : "false");
		return *this;
	}

	PrintWriter& PrintWriter::operator<< (const wchar *str)
	{
		while (*str) {
			*this << *str++;
		}
		return *this;
	}

	PrintWriter& PrintWriter::operator<< (char value)
	{
		write(&value, 1);
		return *this;
	}

	PrintWriter& PrintWriter::operator<< (wchar value)
	{
		// Encode the character as UTF-8
		if (value < 0x80) {
			*this << (char)value;
		} else {
			uint8 Octets[6];
			int OctetsLen = UnicodeUtils::Ucs4ToUtf8((uint32)value, Octets);
			write(Octets, OctetsLen);
		}
		return *this;
	}
	
	PrintWriter& PrintWriter::operator<< (int value)
	{
		wchar buffer[256];
		int len;
		if (MathUtils::convertIntegerToString(value, buffer, len)) {
			*this << buffer;
		}
		return *this;
	}

	PrintWriter& PrintWriter::operator<< (uint64 value)
	{
		wchar buffer[256];
		int len;
		if (MathUtils::convertIntegerToString((sintptr) value, buffer, len)) {
			*this << buffer;
		}
		return *this;
	}

	PrintWriter& PrintWriter::operator<< (int64 value)
	{
		wchar buffer[256];
		int len;
		if (MathUtils::convertIntegerToString((sintptr) value, buffer, len)) {
			*this << buffer;
		}
		return *this;
	}

	PrintWriter& PrintWriter::operator<< (uint32 value)
	{
		wchar buffer[256];
		int len;
		if (MathUtils::convertIntegerToString(value, buffer, len, 10, true)) {
			*this << buffer;
		}
		return *this;
	}

	PrintWriter& PrintWriter::operator<< (double value)
	{
		wchar buffer[256];
		int len;
		MathUtils::convertDoubleToString(value, buffer, len);
		*this << buffer;
		return *this;
	}

	PrintWriter& PrintWriter::operator<< (Stringp str)
	{
		return *this << str->c_str();
	}

	PrintWriter& PrintWriter::operator<< (ScriptObject *obj)
	{
#ifdef AVMPLUS_VERBOSE
		if (obj) {
			return *this << obj->format(m_core);
		} else {
			return *this << "null";
		}
#else
		(void)obj;
		AvmAssert(0); // this is only supported in AVMPLUS_VERBOSE builds
		return *this;
#endif
	}
	PrintWriter& PrintWriter::operator<< (const Traits *obj)
	{
#ifdef AVMPLUS_VERBOSE
		if (obj) {
			return *this << obj->format(m_core);
		} else {
			return *this << "*";
		}
#else
		(void)obj;
		AvmAssert(0); // this is only supported in AVMPLUS_VERBOSE builds
		return *this;
#endif
	}
	PrintWriter& PrintWriter::operator<< (AbstractFunction *obj)
	{
#ifdef AVMPLUS_VERBOSE
		if (obj) {
			return *this << obj->format(m_core);
		} else {
			return *this << "null";
		}
#else
		(void)obj;
		AvmAssert(0); // this is only supported in AVMPLUS_VERBOSE builds
		return *this;
#endif
	}

	PrintWriter& PrintWriter::operator<< (Multiname *obj)
	{
		// Made available in non-AVMPLUS_VERBOSE builds for describeType
//#ifdef AVMPLUS_VERBOSE
#if 1
		if (obj) {
			return *this << obj->format(m_core);
		} else {
			return *this << "null";
		}
#else
		AvmAssert(0); // this is only supported in AVMPLUS_VERBOSE builds
		return *this;
#endif
	}

	PrintWriter& PrintWriter::operator<< (Namespace* ns)
	{
		// Made available in non-AVMPLUS_VERBOSE builds for describeType
//#ifdef AVMPLUS_VERBOSE
#if 1
		if (ns)
			return *this << ns->format(m_core);
		else
			return *this << "null";
#else
		AvmAssert(0); // this is only supported in AVMPLUS_VERBOSE builds
		return *this;
#endif
	}	

	void PrintWriter::writeHexNibble(uint8 value)
	{
		if (value < 10) {
			*this << (char)(value+'0');
		} else {
			*this << (char)(value+'A'-10);
		}
	}
	
	void PrintWriter::writeHexByte(uint8 value)
	{
		writeHexNibble(value>>4);
		writeHexNibble(value&0x0f);
	}

	void PrintWriter::writeHexWord(uint16 value)
	{
		writeHexByte((uint8)(value>>8));
		writeHexByte((uint8)(value&0xff));
	}

	void PrintWriter::writeHexAddr(uintptr value)
	{
#ifdef AVMPLUS_64BIT
		writeHexByte(uint8((value>>56) & 0xff));
		writeHexByte(uint8((value>>48) & 0xff));
		writeHexByte(uint8((value>>40) & 0xff));
		writeHexByte(uint8((value>>32) & 0xff));
#endif
		writeHexByte(uint8((value>>24) & 0xff));
		writeHexByte(uint8((value>>16) & 0xff));
		writeHexByte(uint8(value>>8));
		writeHexByte(uint8(value&0xff));
	}
	
	void PrintWriter::formatTypeName(Traits* t)
	{
		if (!t)
		{
			*this << "*";
			return;
		}

		if (t->base == m_core->traits.class_itraits)
		{
			t = t->itraits;
			*this << "class ";
		}

		if (t->ns != NULL && t->ns != m_core->publicNamespace)
			*this << t->ns << ".";

		if (t->name)
			*this << t->name;
		else
			*this << "(null)";
	}

#ifdef AVMPLUS_VERBOSE
	void PrintWriter::formatP(const char* format, Stringp arg1, Stringp arg2, Stringp arg3)
	{
		while (*format) 
		{
			if (*format == '%') 
			{
				switch (*++format) 
				{
					case '1':
						AvmAssertMsg(arg1!=0, "Expected argument got null");
						if (arg1) *this << arg1;
						break;

					case '2':
						AvmAssertMsg(arg2!=0, "Expected argument got null");
						if (arg2) *this << arg2;
						break;

					case '3':
						AvmAssertMsg(arg3!=0, "Expected argument got null");
						if (arg3) *this << arg3;
						break;

					default:
						AvmAssertMsg(0, "Invalid specifier; should be between '1' and '3'");
						break;
				}
			}
			else
			{
				*this << *format;
			}
			format++;
		}
	}

	void PrintWriter::format(const char *format, ...)
	{
		va_list ap;
		va_start(ap, format);
		formatV(format, ap);
		va_end(ap);
	}

	void PrintWriter::formatV(const char *format, va_list ap)
	{
		while (*format) {
			if (*format == '%') {
				switch (*++format) {
				case 's':
					*this << va_arg(ap, char*);
					break;
				case 'w':
					*this << va_arg(ap, wchar*);
					break;
				case 'a':
					*this << m_core->format(va_arg(ap, Atom));
					break;
				case 'o':
					*this << va_arg(ap, ScriptObject*);
					break;
				case 't': 
					formatTypeName(va_arg(ap, Traits*));
					break;
				case 'm':
					*this << va_arg(ap, AbstractFunction*);
					break;
				case 'n':
					*this << va_arg(ap, Multiname*)->format(m_core, Multiname::MULTI_FORMAT_NAME_ONLY);
					break;
				case 'N':
					*this << va_arg(ap, Multiname*)->format(m_core, Multiname::MULTI_FORMAT_NS_ONLY);
					break;
	#ifdef AVMPLUS_MIR
				case 'A': // addr
	#ifdef AVMPLUS_64BIT
					*this << hexAddr(va_arg(ap,int64));
	#else
					*this << hexAddr(va_arg(ap,int));
	#endif				
					break;

				#ifdef AVMPLUS_ARM
				case 'R': // gp reg
					*this << CodegenMIR::regNames[va_arg(ap, int)];
					break;
				#endif

				#ifdef AVMPLUS_PPC
				case 'R': {// gp reg 
					int r = va_arg(ap,int);
					if (r == CodegenMIR::Unknown)
						*this << "0";
					else
						*this << CodegenMIR::gpregNames[r];
					break;
				}
				case 'F': // fp reg
					*this << CodegenMIR::fpregNames[va_arg(ap, int)];
					break;
                #endif
					
		#if defined (AVMPLUS_IA32) || defined (AVMPLUS_AMD64)
				case 'R': {// gp reg 
					int r = va_arg(ap,int);
					if (r == CodegenMIR::Unknown)
						*this << "0";
					else
						*this << CodegenMIR::gpregNames[r];
					break;
				}
				case 'F': // fp reg
					*this << CodegenMIR::xmmregNames[va_arg(ap, int)];
					break;
				case 'X': // x87 reg
					*this << CodegenMIR::x87regNames[va_arg(ap, int)];
					break;
		#endif
	#endif
				case 'S':
					*this << va_arg(ap, Stringp);
					break;
				case '2':
					{
						// A 2-digit integer.
						int value = va_arg(ap, int);
						*this << (char) ((value/10) + '0');
						*this << (char) ((value%10) + '0');
					}
					break;
				case '3':
					{
						// A 3-char month, used by Date.
						char *str = va_arg(ap, char *);
						*this << str[0] << str[1] << str[2];
					}
					break;
				case 'c':
					{
						// gcc complains if you put va_arg(ap, char)
						char value = (char)(va_arg(ap, int));
						*this << value;
					}
					break;
				case 'f':
					*this << va_arg(ap, double);
					break;
				case 'd':
					*this << va_arg(ap, int);
					break;
				case 'D':
					*this << va_arg(ap, int64);
					break;
				}
			} else {
				*this << *format;
			}
			format++;
		}
		va_end(ap);
	}
#endif /* VERBOSE */
}	
