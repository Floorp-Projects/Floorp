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

#ifndef __avmplus_MethodInfo__
#define __avmplus_MethodInfo__


namespace avmplus
{
#ifdef DEBUGGER
	class AbcFile;
#endif

	/**
	 * The MethodInfo class represents a method or function written
	 * in ActionScript code.
	 */
	class MethodInfo : public AbstractFunction
	{
	public:

#ifdef DEBUGGER
	public:
		int		firstSourceLine;// source line number where function starts
		int		lastSourceLine;	// source line number where function ends
		int     offsetInAbc;	// offset in abc file

		AbcFile* getFile() { return file; }
		void setFile(AbcFile* file) { this->file = file; }

		Stringp getLocalName(int index) const;
		Stringp getArgName(int index) const;
		Stringp getRegName(int index) const;
		void setRegName(int index, Stringp name);

		void boxLocals(void* src, int srcPos, Traits** traitArr, Atom* dest, int destPos, int length);
		void unboxLocals(Atom* src, int srcPos, Traits** traitArr, void* dest, int destPos, int length);
#endif // DEBUGGER
		
		const byte *body_pos;

		// we write this once, in Verifier, with an explicit WB.  so no DWB.
		ExceptionHandlerTable* exceptions;

		MethodInfo();

		static int verifyEnter(MethodEnv* env, int argc, va_list ap);

		void verify(Toplevel* toplevel);

		static int maxScopeDepth(MethodInfo* /*info*/, int max_scope) 
		{
			return max_scope;
		}

#ifdef DEBUGGER
		virtual uint32 size() const;
		// abc size pre-jit, native size post jit
		uint32 codeSize;
		int local_count;
		int max_scopes;
	protected:
		DWB(AbcFile*) file;			// the abc file from which this method came
		DWB(Stringp*) localNames;		// array of names for args and locals in framep order
		void initLocalNames();
#endif // DEBUGGER
	};
}

#endif /* __avmplus_MethodInfo__ */
