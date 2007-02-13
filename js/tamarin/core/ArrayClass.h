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

#ifndef __avmplus_ArrayClass__
#define __avmplus_ArrayClass__


namespace avmplus
{
	/**
	 * class Array
	 */
    class ArrayClass : public ClassClosure
    {
	public:
		ArrayClass(VTable* cvtable);
		
		// this = argv[0]
		// arg1 = argv[1]
		// argN = argv[argc]
		Atom call(int argc, Atom* argv)
		{
			return construct(argc, argv);
		}

		// create a new array, even when argc=1
		ArrayObject* newarray(Atom* argv, int argc);

		ArrayObject* newArray(uint32 capacity=0);

		// override ScriptObject::createInstance
		ArrayObject* createInstance(VTable *ivtable, ScriptObject* prototype);

		ArrayObject* concat(Atom thisAtom, ArrayObject* args);
		Atom pop(Atom thisAtom);
		Atom reverse(Atom thisAtom);
		Atom shift(Atom thisAtom);
		ArrayObject* slice(Atom thisAtom, double A, double B);
		Atom sort(Atom thisAtom, ArrayObject *args);
		Atom sortOn(Atom thisAtom, Atom namesAtom, Atom optionsAtom);
		ArrayObject* splice(Atom thisAtom, ArrayObject* args);

		int indexOf (Atom thisAtom, Atom searchElement, int startIndex);
		int lastIndexOf (Atom thisAtom, Atom searchElement, int startIndex);
		bool every (Atom thisAtom, ScriptObject *callback, Atom thisObject); 
		ArrayObject *filter (Atom thisAtom, ScriptObject *callback, Atom thisObject); 
		void forEach (Atom thisAtom, ScriptObject *callback, Atom thisObject); 
		bool some (Atom thisAtom, ScriptObject *callback, Atom thisObject); 
		ArrayObject *map (Atom thisAtom, ScriptObject *callback, Atom thisObject); 

		DECLARE_NATIVE_MAP(ArrayClass)

		uint32 getLengthHelper (ScriptObject *d);

	private:
		void setLengthHelper (ScriptObject *d, uint32 newLen);

		ArrayObject* isArray(Atom instance)
		{
			if (core()->istype(instance, ivtable()->traits))
				return (ArrayObject*)AvmCore::atomToScriptObject(instance);
			return NULL;
		}

		const DRCWB(Stringp) kComma;
	};
}

#endif /* __avmplus_ArrayClass__ */
