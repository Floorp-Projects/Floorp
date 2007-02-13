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


#include "avmplus.h"

#include "pcre.h"

namespace avmplus
{
	BEGIN_NATIVE_MAP(RegExpClass)
		NATIVE_METHOD(RegExp_source_get, RegExpObject::getSource)
		NATIVE_METHOD(RegExp_global_get, RegExpObject::isGlobal)
		NATIVE_METHOD(RegExp_lastIndex_get, RegExpObject::getLastIndex)
		NATIVE_METHOD(RegExp_lastIndex_set, RegExpObject::setLastIndex)
		NATIVE_METHOD1(RegExp_ignoreCase_get, RegExpObject::hasOption, PCRE_CASELESS)
		NATIVE_METHOD1(RegExp_multiline_get, RegExpObject::hasOption, PCRE_MULTILINE)
		NATIVE_METHOD1(RegExp_dotall_get, RegExpObject::hasOption, PCRE_DOTALL)
		NATIVE_METHOD1(RegExp_extended_get, RegExpObject::hasOption, PCRE_EXTENDED)

		NATIVE_METHOD(RegExp_AS3_exec, RegExpObject::execSimple)
	END_NATIVE_MAP()

	// make pcre's allocators use Flash's
	void *fmalloc(size_t size)
	{
		return new char[size];
	}

	void ffree(void *data)
	{
		char *ptr = (char*) data;
		delete [] ptr;
	}

	RegExpClass::RegExpClass(VTable* cvtable)
		: ClassClosure(cvtable)
	{
		AvmAssert(traits()->sizeofInstance == sizeof(RegExpClass));
		pcre_malloc = &fmalloc;
		pcre_free = &ffree;

		AvmCore* core = this->core();

		ScriptObject* object_prototype = toplevel()->objectClass->prototype;
		prototype = new (core->GetGC(), ivtable()->getExtraSize()) RegExpObject(this,object_prototype);

		kindex = core->constant("index");
		kinput = core->constant("input");		
	}
	
	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom RegExpClass::call(int argc, Atom* argv)
	{
		// ECMA-262 15.10.4.1: If pattern is RegExp and flags is undefined,
		// return pattern unchanged.
		if (argc > 0) {
			Atom flagsAtom = (argc>1) ? argv[2] : undefinedAtom;
			if (core()->istype(argv[1], traits()->itraits) && flagsAtom == undefinedAtom) {
				return argv[1];
			}
		}

		// Otherwise, call the RegExp constructor.
		return construct(argc, argv);
	}

	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom RegExpClass::construct(int argc, Atom* argv)
	{
		AvmCore* core = this->core();
		Stringp pattern;

		Atom patternAtom = (argc>0) ? argv[1] : undefinedAtom;
		Atom optionsAtom = (argc>1) ? argv[2] : undefinedAtom;

		if (core->istype(patternAtom, traits()->itraits)) {
			// Pattern is a RegExp object
			if (optionsAtom != undefinedAtom) {
				// ECMA 15.10.4.1 says to throw an error if flags specified
				toplevel()->throwTypeError(kRegExpFlagsArgumentError);
			}
			// Return a clone of the RegExp object
			RegExpObject* regExpObject = (RegExpObject*)AvmCore::atomToScriptObject(patternAtom);
			return (new (core->GetGC(), ivtable()->getExtraSize()) RegExpObject(regExpObject))->atom();
		} else {
			if (patternAtom != undefinedAtom) {
				pattern = core->string(argv[1]);
			} else {
				// cn:  disable this, breaking ecma3 tests.   was: todo look into this. it's what SpiderMonkey does.
				pattern = core->kEmptyString; //core->newString("(?:)");
			}
		}

		Stringp options = NULL;
		if (optionsAtom != undefinedAtom) {
			options = core->string(optionsAtom);
		}

		RegExpObject* inst = new (core->GetGC(), ivtable()->getExtraSize()) RegExpObject(this, pattern, options);
		return inst->atom();
	}
}
