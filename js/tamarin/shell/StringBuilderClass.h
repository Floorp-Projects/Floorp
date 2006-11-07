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

#ifndef __avmshell_StringBuilderClass__
#define __avmshell_StringBuilderClass__


namespace avmshell
{
	class StringBuilderObject : public ScriptObject
	{
	public:		
		StringBuilderObject(VTable *vtable, ScriptObject *delegate);
		~StringBuilderObject();

		void append(Atom value);
		uint32 get_capacity();
		Stringp charAt(uint32 index);
		uint32 charCodeAt(uint32 index);
		void ensureCapacity(uint32 minimumCapacity);
		int indexOf(Stringp str, uint32 index);
		void insert(uint32 offset, Atom value);
		int lastIndexOf(Stringp substr, uint32 index);
		uint32 get_length();
		void set_length(uint32 length);
		void remove(uint32 start, uint32 end);
		void removeCharAt(uint32 index);
		void replace(uint32 start, uint32 end, Stringp replacement);
		void reverse();
		void setCharAt(uint32 index, Stringp ch);
		Stringp substring(uint32 start, uint32 end);
		Stringp _toString();
		void trimToSize();

	private:
		wchar *m_buffer;
		uint32 m_length;
		uint32 m_capacity;

		static const uint32 kInitialCapacity = 16;
	};

	class StringBuilderClass : public ClassClosure
	{
    public:
		StringBuilderClass(VTable* cvtable);
		ScriptObject *createInstance(VTable *ivtable, ScriptObject *delegate);
		DECLARE_NATIVE_MAP(StringBuilderClass)
	};
}

#endif /* __avmshell_StringBuilderClass__ */
