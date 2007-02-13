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

#ifndef __avmplus_ClassClosure__
#define __avmplus_ClassClosure__


namespace avmplus
{
	/**
	 * a user defined class, ie class MyClass
	 */
	class ClassClosure : public ScriptObject
	{
	public:
		DRCWB(ScriptObject*) prototype;

		ClassClosure(VTable *cvtable);

		Atom get_prototype();
		void set_prototype(Atom p);

		void createVanillaPrototype();

		/**
		 * called as constructor, as in new C().  for user classes this
		 * invokes the implicit constructor followed by the user's constructor
		 * if any.
		 */
		virtual Atom construct(int argc, Atom* argv);

		/**
		 * called as function, as in C().  For user classes, this is the
		 * the explicit coersion function.  For user functions, we
		 * invoke m_call.
		 */
		virtual Atom call(int argc, Atom* argv);

		// Accessors for Function.length
		int get_length();

		VTable* ivtable() const;

#ifdef DEBUGGER
		uint32 size() const;
#endif

#ifdef AVMPLUS_VERBOSE
	public:
		Stringp format(AvmCore* core) const;
#endif
	};
}

#endif /* __avmplus_ClassClosure__ */
