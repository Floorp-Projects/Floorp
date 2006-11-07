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
 * by the Initial Developer are Copyright (C)[ 1993-2006 ] Adobe Systems Incorporated. All Rights 
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


#ifndef __avmplus_StringBuffer__
#define __avmplus_StringBuffer__

namespace avmplus
{
	/**
	 * StringOutputStream is an OutputStream subclass, where
	 * any data written to the stream is accumulated into
	 * an in-memory buffer.  The buffer can later be retrieved
	 * using the c_str method.
	 */
	class StringOutputStream : public OutputStream
	{
	public:
		StringOutputStream(MMgc::GC *gc);
		~StringOutputStream();

		const char *c_str() { return m_buffer; }
		int length() const { return m_length; }
		void reset() { m_length=0; }

		int write(const void *buffer, int count);
		
	private:
		enum { kInitialCapacity = 256 };
		
		char* m_buffer;
		int m_length;

		void grow(int newLength);
	};

	/**
	 * StringBuffer is a PrintWriter subclass which enables
	 * easy, cout-like output of text to a string buffer
	 * in memory.
	 */
	class StringBuffer : public PrintWriter
	{
	public:
		StringBuffer(AvmCore* core) :
			PrintWriter(core), m_stream(core->GetGC())
		{
			setOutputStream(&m_stream);
		}

		StringBuffer(MMgc::GC *gc) :
			PrintWriter(NULL), m_stream(gc)
		{
			setOutputStream(&m_stream);
		}

		const char *c_str() { return m_stream.c_str(); }
		int length() const { return m_stream.length(); }
		void reset() { m_stream.reset(); }
		
	private:
		StringOutputStream m_stream;
	};		
}
		
#endif /* __avmplus_StringBuffer__ */
