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



#include "avmshell.h"

namespace avmshell
{
	//
	// ShortArrayClass
	//

	BEGIN_NATIVE_MAP(ShortArrayClass)
		NATIVE_METHOD(flash_utils_ShortArray_length_get,        ShortArrayObject::get_length)
		NATIVE_METHOD(flash_utils_ShortArray_length_set,        ShortArrayObject::set_length)
	END_NATIVE_MAP()

	ShortArrayClass::ShortArrayClass(VTable *vtable)
		: ClassClosure(vtable)
    {
        prototype = toplevel()->objectClass->construct();
	}

	ScriptObject* ShortArrayClass::createInstance(VTable *ivtable,
												 ScriptObject *prototype)
    {
        return new (core()->GetGC(), ivtable->getExtraSize()) ShortArrayObject(ivtable, prototype);
    }

	//
	// UShortArrayClass
	//

	BEGIN_NATIVE_MAP(UShortArrayClass)
		NATIVE_METHOD(flash_utils_UShortArray_length_get,        UShortArrayObject::get_length)
		NATIVE_METHOD(flash_utils_UShortArray_length_set,        UShortArrayObject::set_length)
	END_NATIVE_MAP()

	UShortArrayClass::UShortArrayClass(VTable *vtable)
		: ClassClosure(vtable)
    {
        prototype = toplevel()->objectClass->construct();
	}

	ScriptObject* UShortArrayClass::createInstance(VTable *ivtable,
												 ScriptObject *prototype)
    {
        return new (core()->GetGC(), ivtable->getExtraSize()) UShortArrayObject(ivtable, prototype);
    }

	//
	// IntArrayClass
	//

	BEGIN_NATIVE_MAP(IntArrayClass)
		NATIVE_METHOD(flash_utils_IntArray_length_get,        IntArrayObject::get_length)
		NATIVE_METHOD(flash_utils_IntArray_length_set,        IntArrayObject::set_length)
	END_NATIVE_MAP()

	IntArrayClass::IntArrayClass(VTable *vtable)
		: ClassClosure(vtable)
    {
        prototype = toplevel()->objectClass->construct();
	}

	ScriptObject* IntArrayClass::createInstance(VTable *ivtable,
												 ScriptObject *prototype)
    {
        return new (core()->GetGC(), ivtable->getExtraSize()) IntArrayObject(ivtable, prototype);
    }

	//
	// UIntArrayClass
	//

	BEGIN_NATIVE_MAP(UIntArrayClass)
		NATIVE_METHOD(flash_utils_UIntArray_length_get,        UIntArrayObject::get_length)
		NATIVE_METHOD(flash_utils_UIntArray_length_set,        UIntArrayObject::set_length)
	END_NATIVE_MAP()

	UIntArrayClass::UIntArrayClass(VTable *vtable)
		: ClassClosure(vtable)
    {
        prototype = toplevel()->objectClass->construct();
	}

	ScriptObject* UIntArrayClass::createInstance(VTable *ivtable,
												 ScriptObject *prototype)
    {
        return new (core()->GetGC(), ivtable->getExtraSize()) UIntArrayObject(ivtable, prototype);
    }

	//
	// FloatArrayClass
	//

	BEGIN_NATIVE_MAP(FloatArrayClass)
		NATIVE_METHOD(flash_utils_FloatArray_length_get,        FloatArrayObject::get_length)
		NATIVE_METHOD(flash_utils_FloatArray_length_set,        FloatArrayObject::set_length)
	END_NATIVE_MAP()

	FloatArrayClass::FloatArrayClass(VTable *vtable)
		: ClassClosure(vtable)
    {
        prototype = toplevel()->objectClass->construct();
	}

	ScriptObject* FloatArrayClass::createInstance(VTable *ivtable,
												 ScriptObject *prototype)
    {
        return new (core()->GetGC(), ivtable->getExtraSize()) FloatArrayObject(ivtable, prototype);
    }

	//
	// DoubleArrayClass
	//

	BEGIN_NATIVE_MAP(DoubleArrayClass)
		NATIVE_METHOD(flash_utils_DoubleArray_length_get,        DoubleArrayObject::get_length)
		NATIVE_METHOD(flash_utils_DoubleArray_length_set,        DoubleArrayObject::set_length)
	END_NATIVE_MAP()

	DoubleArrayClass::DoubleArrayClass(VTable *vtable)
		: ClassClosure(vtable)
    {
        prototype = toplevel()->objectClass->construct();
	}

	ScriptObject* DoubleArrayClass::createInstance(VTable *ivtable,
												   ScriptObject *prototype)
    {
        return new (core()->GetGC(), ivtable->getExtraSize()) DoubleArrayObject(ivtable, prototype);
    }
}
