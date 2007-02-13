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
 * Portions created by the Initial Developer are Copyright (C) 1993-2006
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

namespace avmplus
{
	BuiltinTraits::BuiltinTraits()
	{
		memset(this, 0, sizeof(BuiltinTraits));
	}

	// this is called after core types are defined.  we don't want to do
	// it earlier because we need Void and int
	void BuiltinTraits::initInstanceTypes(PoolObject* pool)
	{
		AvmCore* core = pool->core;

		class_itraits		= pool->getBuiltinTraits(core->constantString("Class"));
		namespace_itraits	= pool->getBuiltinTraits(core->constantString("Namespace"));
		function_itraits	= pool->getBuiltinTraits(core->constantString("Function"));
		boolean_itraits		= pool->getBuiltinTraits(core->constantString("Boolean"));
		number_itraits		= pool->getBuiltinTraits(core->constantString("Number"));
		int_itraits			= pool->getBuiltinTraits(core->constantString("int"));
		uint_itraits		= pool->getBuiltinTraits(core->constantString("uint"));
		string_itraits		= pool->getBuiltinTraits(core->constantString("String"));
		array_itraits		= pool->getBuiltinTraits(core->constantString("Array"));
		regexp_itraits		= pool->getBuiltinTraits(core->constantString("RegExp"));
		date_itraits		= pool->getBuiltinTraits(core->constantString("Date"));
		error_itraits		= pool->getBuiltinTraits(core->constantString("Error"));		
		qName_itraits		= pool->getBuiltinTraits(core->constantString("QName"));
		xml_itraits			= pool->getBuiltinTraits(core->constantString("XML"));
		xmlList_itraits     = pool->getBuiltinTraits(core->constantString("XMLList"));

		null_itraits = core->newTraits(NULL, 0, 0, 0);
		null_itraits->pool = pool;
		null_itraits->ns = core->publicNamespace;
		null_itraits->name = core->constantString("null");
		null_itraits->final = true;
		null_itraits->linked = true;

		void_itraits = core->newTraits(NULL, 0, 0, 0);
		void_itraits->pool = pool;
		void_itraits->ns = core->publicNamespace;
		void_itraits->name = core->constantString("void");
		void_itraits->final = true;
		void_itraits->linked = true;

		// we have fast equality checks in the core that only work
		// because these two classes are final.  If they become non-final
		// then these checks need to be updated to account for possible
		// subclassing.
		AvmAssert(xml_itraits->final);
		AvmAssert(xmlList_itraits->final);

		object_itraits->isMachineType	= true;
		void_itraits->isMachineType		= true;
		int_itraits->isMachineType		= true;
		uint_itraits->isMachineType		= true;
		boolean_itraits->isMachineType	= true;
		number_itraits->isMachineType	= true;

		int_itraits->isNumeric			= true;
		uint_itraits->isNumeric			= true;
		number_itraits->isNumeric		= true;

		// XML and XMLList are dynamic but do not need the
		// standard dynamic hash table
		xml_itraits->needsHashtable     = false;
		xmlList_itraits->needsHashtable = false;

		// All these types including XML, XMLList and QName
		// are marked as not derived objects.
		//array_itraits->notDerivedObjectOrXML	= true;
		boolean_itraits->notDerivedObjectOrXML	= true;
		class_itraits->notDerivedObjectOrXML	= true;
		//date_itraits->notDerivedObjectOrXML	= true;
		function_itraits->notDerivedObjectOrXML	= true;
		namespace_itraits->notDerivedObjectOrXML= true;
		null_itraits->notDerivedObjectOrXML		= true;
		number_itraits->notDerivedObjectOrXML	= true;
		int_itraits->notDerivedObjectOrXML		= true;
		uint_itraits->notDerivedObjectOrXML		= true;
		object_itraits->notDerivedObjectOrXML	= true;
		//regexp_itraits->notDerivedObjectOrXML	= true;
		string_itraits->notDerivedObjectOrXML	= true;
		//toplevel_itraits->notDerivedObjectOrXML	= true;
		void_itraits->notDerivedObjectOrXML		= true;
		xml_itraits->notDerivedObjectOrXML		= true;
		xmlList_itraits->notDerivedObjectOrXML	= true;
		qName_itraits->notDerivedObjectOrXML	= true;

		Traits* methodClosure_itraits;
		methodClosure_itraits = pool->getBuiltinTraits(core->constantString("MethodClosure"));
		methodClosure_itraits->notDerivedObjectOrXML = true;
	}

	void BuiltinTraits::initClassTypes(PoolObject* pool)
	{
		math_ctraits = findCTraits("Math$", pool);
		number_ctraits = findCTraits("Number$", pool);
		int_ctraits = findCTraits("int$", pool);
		uint_ctraits = findCTraits("uint$", pool);
		boolean_ctraits = findCTraits("Boolean$", pool);
		string_ctraits = findCTraits("String$", pool);
	}

	Traits* BuiltinTraits::findCTraits(const char* cname, PoolObject* pool)
	{
		Stringp name = pool->core->constantString(cname);
		for (int i=0, n=pool->cinits.capacity(); i < n; i++) 
		{
			AbstractFunction* cinit = pool->cinits[i];
			if (cinit && cinit->declaringTraits->name == name) 
			{
				return cinit->declaringTraits;
			}
		}
		return NULL;
	}
}
