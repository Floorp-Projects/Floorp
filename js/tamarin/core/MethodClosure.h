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

#ifndef __avmplus_MethodClosure__
#define __avmplus_MethodClosure__


namespace avmplus
{
	class MethodClosureClass : public ClassClosure
	{
	public:
		DECLARE_NATIVE_MAP(MethodClosureClass)

		MethodClosureClass(VTable* cvtable);

		// Function called as constructor ... not supported from user code
		// this = argv[0] (ignored)
		// arg1 = argv[1]
		// argN = argv[argc]
		Atom construct(int /*argc*/, Atom* /*argv*/)
		{
			AvmAssert(false);
			return nullObjectAtom;
		}

		Atom call(int argc, Atom* argv) 
		{
			return construct(argc,argv);
		}

		MethodClosure* create(MethodEnv* env, Atom savedThis);
	};

	/**
	 * The MethodClosure class is used to represent method
	 * closures; that is, a function which is a method of
	 * a class.
	 *
	 * MethodClosure is how AVM+ supports the "method extraction"
	 * behavior of ECMAScript Edition 4.  When a reference to
	 * a method of a class is retrieved, a MethodClosure is
	 * created.  The MethodClosure also remembers the object
	 * instance that the method was extracted from.  When the
	 * MethodClosure is invoked, the method is invoked with
	 * "this" pointing to the remembered instance.
	 */
	class MethodClosure : public ScriptObject
	{
		friend class MethodClosureClass;
		MethodClosure(VTable* vtable, ScriptObject* prototype, MethodEnv* env, Atom savedThis);
	public:
		DWB(MethodEnv*) env;
		ATOM_WB savedThis;


		// argc is args only, argv[0] = receiver
		Atom call(int argc, Atom* argv);

		// argc is args only, argv[0] = receiver(ignored)
		Atom construct(int argc, Atom* argv);

		int get_length() const;

		bool isMethodClosure() { return true; }

#ifdef AVMPLUS_VERBOSE
	public:
		Stringp format(AvmCore* core) const;
#endif
	};
}

#endif /* __avmplus_MethodClosure__ */
