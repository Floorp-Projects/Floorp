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
 * by the Initial Developer are Copyright (C)[ 1993-2006 ] Adobe Systems Incorporated. All Rights 
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


#ifndef __avmplus_BuiltinTraits__
#define __avmplus_BuiltinTraits__

namespace avmplus
{
	/**
	 * BuiltinTraits is a container for the traits of all of
	 * the built-in objects.
	 *
	 * There may be multiple Toplevel objects, each with their
	 * own closures for the core objects.  Multiple Toplevels are
	 * used in the Flash Player for security reasons.  Thus, there
	 * may be multiple closures for the same core class.
	 * There will only be one set of traits, however, and traits
	 * are immutable.  So, BuiltinTraits is global to the entire
	 * VM.
	 */
	class BuiltinTraits
	{
	public:
		BuiltinTraits();

		void initInstanceTypes(PoolObject* pool);
		void initFunctionTypes();
		void initClassTypes(PoolObject* pool);

		// Main built-in class traits
		Traits *array_itraits;
		Traits *boolean_itraits;
		Traits *class_itraits;
		Traits *date_itraits;
		Traits *error_itraits;
		Traits *function_itraits;
		Traits *namespace_itraits;
		Traits *null_itraits;
		Traits *number_itraits;
		Traits *int_itraits;
		Traits *uint_itraits;
		Traits *object_itraits;
		Traits *regexp_itraits;
		Traits *string_itraits;
		Traits *toplevel_itraits;
		Traits *void_itraits;
		Traits *xml_itraits;
		Traits *xmlList_itraits;
		Traits *qName_itraits;

		Traits* math_ctraits;
		Traits* int_ctraits;
		Traits* uint_ctraits;
		Traits* number_ctraits;
		Traits* string_ctraits;
		Traits* boolean_ctraits;

	private:
		Traits* findCTraits(const char* name, PoolObject* pool);
	};
}

#endif /* __avmplus_BuiltinTraits__ */
