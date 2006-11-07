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


#include "avmplus.h"

namespace avmplus
{
	BEGIN_NATIVE_MAP(ErrorClass)
		NATIVE_METHOD(Error_getStackTrace, ErrorObject::stackTraceToString)
		NATIVE_METHOD(Error_getErrorMessage, ErrorClass::getErrorMessage)
	END_NATIVE_MAP()

	ErrorClass::ErrorClass(VTable* cvtable)
		: ClassClosure(cvtable)
	{
		AvmAssert(traits()->sizeofInstance == sizeof(ErrorClass));

		prototype = createInstance(ivtable(), toplevel()->objectClass->prototype);
	}
	
	/**
	 * ErrorObject
	 */
	ErrorObject::ErrorObject(VTable* vtable,
							 ScriptObject *delegate)
		: ScriptObject(vtable, delegate)
	{
		AvmAssert(traits()->sizeofInstance == sizeof(ErrorObject));

		#ifdef DEBUGGER
		AvmCore *core = this->core();
		// Copy the stack trace
		stackTrace = core->newStackTrace();
		#endif
	}

	void ErrorClass::throwError(int errorID, Stringp arg1, Stringp arg2, Stringp arg3)
	{
		core()->throwErrorV(this, errorID, arg1, arg2, arg3);
	}

	Stringp ErrorObject::stackTraceToString() const
	{
		#ifdef DEBUGGER
		AvmCore* core = this->core();

		// getStackTrace returns the concatenation of the
		// error message and the stack trace
		Stringp buffer = core->string(atom());
		buffer = core->concatStrings(buffer, core->newString("\n"));

		if (stackTrace) {
			buffer = core->concatStrings(buffer, stackTrace->format(core));
		}

		return buffer;
		#else
		return NULL;
		#endif
	}

	Stringp ErrorClass::getErrorMessage(int errorID) const
	{
		return this->core()->getErrorMessage(errorID);
	}

	ScriptObject* ErrorClass::createInstance(VTable *ivtable,
											 ScriptObject *prototype)
	{
		return new (ivtable->gc(), ivtable->getExtraSize()) ErrorObject(ivtable, prototype);
	}

	/**
	 * NativeErrorClass
	 */

	BEGIN_NATIVE_MAP(NativeErrorClass)
	END_NATIVE_MAP()	

	NativeErrorClass::NativeErrorClass(VTable* cvtable)
		: ClassClosure(cvtable)
	{
		AvmAssert(traits()->sizeofInstance == sizeof(ErrorClass));
		createVanillaPrototype();
	}
}
