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

#ifndef __avmplus_PrintWriter__
#define __avmplus_PrintWriter__


namespace avmplus
{
	/**
	 * hexDWord is an operator that can be used with PrintWriter
	 * to write out a dword in hex
	 */
	class hexDWord
	{
	public:
		hexDWord(uint32 value) { this->value = value; }
		hexDWord(const hexDWord& toCopy) { value = toCopy.value; }
		hexDWord& operator= (const hexDWord& toCopy) {
			value = toCopy.value;
			return *this;
		}
		uint32 getValue() const { return value; }
		
	private:
		uint32 value;
	};

	/**
	 * tabstop is an operator that can be used with PrintWriter
	 * to advance to the specified tabstop
	 */
	class tabstop
	{
	public:
		tabstop(int spaces) { this->spaces = spaces; }
		tabstop(const tabstop& toCopy) { spaces = toCopy.spaces; }
		tabstop& operator= (const tabstop& toCopy) {
			spaces = toCopy.spaces;
			return *this;
		}
		int getSpaces() const { return spaces; }
		
	private:
		int spaces;
	};
	
	/**
	 * percent is an operator that can be used with PrintWrtier
	 * to output a number as a percentage
	 */
	class percent
	{
	public:
		percent(double value) { this->value = value; }
		percent(const percent& toCopy) { value = toCopy.value; }
		percent& operator= (const percent& toCopy) {
			value = toCopy.value;
			return *this;
		}
		double getPercent() const { return value; }
		
	private:
		double value;
	};

	/**
	 * PrintWriter is a utility class for writing human-readable
	 * text.  It has an interface similar to the C++ iostreams
	 * library, overloading the "<<" operator to accept most
	 * standard types used in the VM.
	 */
	class PrintWriter : public OutputStream
	{
	public:
		PrintWriter(AvmCore* core) { m_core = core; m_stream = NULL; col = 0; }
		PrintWriter(AvmCore* core, OutputStream *stream) { m_core = core; m_stream = stream; col = 0; }
		~PrintWriter() {}

		void setOutputStream(OutputStream *stream) { m_stream = stream; }
		void setCore(AvmCore* core) { m_core = core; }
		
		int write(const void *buffer, int count);

		PrintWriter& operator<< (const char *str);
		PrintWriter& operator<< (const wchar *str);
		PrintWriter& operator<< (char value);
		PrintWriter& operator<< (wchar value);		
		PrintWriter& operator<< (int value);
		PrintWriter& operator<< (uint64 value);
		PrintWriter& operator<< (uint32 value);
		PrintWriter& operator<< (double value);
		PrintWriter& operator<< (Stringp str);
		PrintWriter& operator<< (tabstop tabs);
		PrintWriter& operator<< (hexDWord tabs);
		PrintWriter& operator<< (percent value);		
		PrintWriter& operator<< (bool b);

		void formatTypeName(Traits* t);

		void writeHexByte(uint8 value);
		void writeHexWord(uint16 value);
		void writeHexDWord(uint32 value);

		#ifdef AVMPLUS_VERBOSE
		void format(const char *format, ...);
		void formatV(const char *format, va_list ap);
		void formatP(const char* format, Stringp arg1=0, Stringp arg2=0, Stringp arg3=0);
		#endif
		
	private:
		int col;
		OutputStream *m_stream;
		AvmCore *m_core;

		void writeHexNibble(uint8 value);	

		// These are defined for not DEBUGGER builds but fire asserts
	public:
		PrintWriter& operator<< (ScriptObject* obj);
		PrintWriter& operator<< (const Traits* obj);
		PrintWriter& operator<< (AbstractFunction* obj);
		PrintWriter& operator<< (Multiname* obj);
		PrintWriter& operator<< (Namespace* str);
	};
}

#endif /* __avmplus_PrintWriter__ */
