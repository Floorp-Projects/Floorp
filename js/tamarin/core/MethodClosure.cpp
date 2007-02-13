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

using namespace MMgc;

namespace avmplus
{
	// [ed] why does the E4 semantics MethodClosure have a slot table?
	// [jd] The slot table of a method closure is used to hold the value of the length
	// property for the method. Dont know how important this is, but there you go.
	// [ed] 9/15/04 we're implementing length as a getter instead of a slot.

    MethodClosure::MethodClosure(VTable* ivtable, ScriptObject* prototype, 
								 MethodEnv *env, Atom savedThis)
		: ScriptObject(ivtable, prototype)
    {
		AvmAssert(traits()->sizeofInstance == sizeof(MethodClosure));
		AvmAssert(env != NULL);
		AvmAssert(!AvmCore::isNullOrUndefined(savedThis));
        this->env = env;
        this->savedThis = savedThis;
    }

	// this = argv[0] (ignored, we use savedThis instead)
	// arg1 = argv[1]
	// argN = argv[argc]
    Atom MethodClosure::call(int argc, Atom* argv)
    {
		argv[0] = savedThis;
		return env->coerceEnter(argc, argv);
    }

	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
    Atom MethodClosure::construct(int /*argc*/, Atom* /*argv*/)
    {
        // can't invoke method closure as constructor:
        //     m = o.m;
        //     new m(); // error
		toplevel()->throwTypeError(kCannotCallMethodAsConstructor, core()->toErrorString(env->method));		
        return undefinedAtom;
    }

	int MethodClosure::get_length() const
	{
		return env->method->param_count;
	}

#ifdef AVMPLUS_VERBOSE
    Stringp MethodClosure::format(AvmCore* core) const
    {
		Stringp prefix = core->newString("MC{");
		prefix = core->concatStrings(prefix, core->format(savedThis));
		prefix = core->concatStrings(prefix, core->newString(" "));
		prefix = core->concatStrings(prefix, env->method->format(core));
		prefix = core->concatStrings(prefix, core->newString("}@"));
		return core->concatStrings(prefix, core->formatAtomPtr(atom()));
    }
#endif

	BEGIN_NATIVE_MAP(MethodClosureClass)
		NATIVE_METHOD2(builtin_as_0_MethodClosure_length_get, &MethodClosure::get_length)
	END_NATIVE_MAP()

	MethodClosureClass::MethodClosureClass(VTable* cvtable)
		: ClassClosure(cvtable)
	{
		Toplevel* toplevel = this->toplevel();

		toplevel->methodClosureClass = this;
		AvmAssert(traits()->sizeofInstance == sizeof(MethodClosureClass));

		prototype = toplevel->functionClass->createEmptyFunction();
	}

	// Function called as constructor ... not supported from user code
	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	MethodClosure* MethodClosureClass::create(MethodEnv* m, Atom obj)
	{		
		WeakKeyHashtable *mcTable = m->getMethodClosureTable();		
		Atom mcWeakAtom = mcTable->get(obj);
		GCWeakRef *ref = (GCWeakRef*)AvmCore::atomToGCObject(mcWeakAtom);
		MethodClosure *mc;

		if(!ref || !ref->get())
		{
			VTable* ivtable = this->ivtable();
			mc = (new (core()->GetGC(), ivtable->getExtraSize()) MethodClosure(ivtable, prototype, m, obj));
			mcWeakAtom = AvmCore::gcObjectToAtom(mc->GetWeakRef());
			mcTable->add(obj, mcWeakAtom);
		}
		else
		{
			mc = (MethodClosure*)ref->get();
		}
		return mc;
	}
}
