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
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
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

import ExtractMethPackage.*;


var SECTION = "Method Closures";       							// provide a document reference (ie, ECMA section)
var VERSION = "AS 3.0";  								// Version of JavaScript or ECMA
var TITLE   = "Extract methods from a public class";    	// Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                							// leave this alone




var obj:PubClassExtractMeth = new PubClassExtractMeth();

// *****************************
// Extract a public method
// *****************************
var mc : Function = obj.pubGetX;
AddTestCase( "*** extract a public function from a public class", 1, 1 );
AddTestCase( "obj.pubGetX() == mc()", obj.pubGetX(), mc() );


// *****************************
// Extract a static method
// *****************************
var mc2 : Function = PubClassExtractMeth.statGetX;
AddTestCase( "*** extract a static function from a public class", 1, 1 );
AddTestCase( "PubClassExtractMeth.statGetX() == mc2()", PubClassExtractMeth.statGetX(), mc2() );


test();       		// Leave this function alone.
			// This function is for executing the test case and then
			// displaying the result on to the console or the LOG file.
