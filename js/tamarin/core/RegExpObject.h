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

#ifndef __avmplus_RegExpObject__
#define __avmplus_RegExpObject__

namespace avmplus
{
	/**
	 * The RegExpObject class is the C++ implementation of instances
	 * of the "RegExp" class, as defined in the ECMAScript standard.
	 */
	class RegExpObject : public ScriptObject
	{
	public:
		/** This variant is used to create RegExp.prototype	*/
		RegExpObject(RegExpClass *arrayClass, ScriptObject *delegate);

		RegExpObject(RegExpClass *type,
					 Stringp pattern,
					 Stringp options);
		
		/* Copy constructor */			 
		RegExpObject(RegExpObject *toCopy);
		
		~RegExpObject();

		// call is implicit exec
		// atom[0] = this
		// atom[1] = arg1
		// atom[argc] = argN
		Atom call(int argc, Atom* argv);

		int search(Stringp subject);
		ArrayObject* split(Stringp subject, uint32 limit);
		ArrayObject* match(Stringp subject);
		
		Atom execSimple(Stringp subject);
		Atom replace(Stringp subject, Stringp replacement);
		Atom replace(Stringp subject, ScriptObject *replaceFunction);

		Stringp getSource() const { return m_source; }
		bool isGlobal() const { return m_global; }
		int getLastIndex() const { return m_lastIndex; }
		void setLastIndex(int newIndex) { m_lastIndex = newIndex; }
		bool hasOption(int mask) { return (m_optionFlags&mask) != 0; }
		
	private:
		DRCWB(Stringp) m_source;
		bool           m_global;
		int            m_lastIndex;
		int            m_optionFlags;
		bool           m_hasNamedGroups;
		void*          m_pcreInst;

		Atom stringFromUTF8(const char *buffer, int len);

		ArrayObject* exec(Stringp subject, UTF8String *utf8Subject);
		
		ArrayObject* exec(Stringp subject,
						  UTF8String *utf8Subject,
						  int startIndex,
						  int& matchIndex,
						  int& matchLen);
		
		int Utf16ToUtf8Index(Stringp utf16String,
							 UTF8String *utf8String,
							 int utf16Index);

		int Utf8ToUtf16Index(Stringp utf16String,
							 UTF8String *utf8String,
							 int utf8Index);

		void fixReplaceLastIndex(const char *src,
								 int subjectLength,
								 int lastIndex,
								 int& newLastIndex,
								 StringBuffer& resultBuffer);

		int numBytesInUtf8Character(const uint8 *in);

	};
}

#endif /* __avmplus_RegExpObject__ */
