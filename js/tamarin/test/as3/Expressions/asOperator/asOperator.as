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

var SECTION = "Expressions";        // provide a document reference (ie, Actionscript section)
var VERSION = "AS3";        		// Version of ECMAScript or ActionScript
var TITLE   = "as operator";        // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

// as Array
AddTestCase( "null as Array", null, (null as Array));
AddTestCase( "[1,1,1,1] as Array", "1,1,1,1", ([1,1,1,1] as Array).toString());
AddTestCase( "var array = new Array('1'); array as Array", "1", (array = new Array('1'), (array as Array).toString()));
AddTestCase( "Boolean as Array", null, (Boolean as Array));
AddTestCase( "Date as Array", null, (Date as Array));
AddTestCase( "Function as Array", null, (Function as Array));
AddTestCase( "Math as Array", null, (Math as Array));
AddTestCase( "Number as Array", null, (Number as Array));
AddTestCase( "Object as Array", null, (Object as Array));
AddTestCase( "String as Array", null, (String as Array));
AddTestCase( "RegExp as Array", null, (RegExp as Array));
AddTestCase( "int as Array", null, (int as Array));
AddTestCase( "uint as Array", null, (uint as Array));

// as Boolean
AddTestCase( "null as Boolean", null, (null as Boolean));
AddTestCase( "false as Boolean", false, (false as Boolean));
AddTestCase( "true as Boolean", true, (true as Boolean));
AddTestCase( "Array as Boolean", null, (Array as Boolean));
AddTestCase( "Date as Boolean", null, (Date as Boolean));
AddTestCase( "Function as Boolean", null, (Function as Boolean));
AddTestCase( "Math as Boolean", null, (Math as Boolean));
AddTestCase( "Number as Boolean", null, (Number as Boolean));
AddTestCase( "Object as Boolean", null, (Object as Boolean));
AddTestCase( "String as Boolean", null, (String as Boolean));
AddTestCase( "RegExp as Boolean", null, (RegExp as Boolean));
AddTestCase( "int as Boolean", null, (int as Boolean));
AddTestCase( "uint as Boolean", null, (uint as Boolean));

// as Date
AddTestCase( "null as Date", null, (null as Date));
AddTestCase( "undefined as Date", null, (undefined as Date));
AddTestCase( "'test' as Date", null, ('test' as Date));
// fails if not PDT
//AddTestCase( "new Date(0) as Date", "Wed Dec 31 16:00:00 GMT-0800 1969", (new Date(0) as Date).toString());
AddTestCase( "'Wed Dec 31 16:00:00 GMT-0800 1969' as Date", null, ("Wed Dec 31 16:00:00 GMT-0800 1969" as Date));
AddTestCase( "Array as Date", null, (Array as Date));
AddTestCase( "Boolean as Date", null, (Boolean as Date));
AddTestCase( "Function as Date", null, (Function as Date));
AddTestCase( "Math as Date", null, (Math as Date));
AddTestCase( "Number as Date", null, (Number as Date));
AddTestCase( "Object as Date", null, (Object as Date));
AddTestCase( "String as Date", null, (String as Date));
AddTestCase( "''as Date", null, ("" as Date));
AddTestCase( "RegExp as Date", null, (RegExp as Date));
AddTestCase( "int as Date", null, (int as Date));
AddTestCase( "uint as Date", null, (uint as Date));
//AddTestCase( "void as Date", null, (void as Date));
AddTestCase( "Date as Date", null, (Date as Date));



// as Function
AddTestCase( "null as Function ", null, (null as Function));

// as Math
AddTestCase( "null as Math", null, (null as Math));

// as Number
AddTestCase( "null as Number", null, (null as Number));
AddTestCase( "1 as Number", 1, (1 as Number));
AddTestCase( "1.66 as Number", 1.66, (1.66 as Number));
AddTestCase( "-1.66 as Number", -1.66, (-1.66 as Number));

// as Object
AddTestCase( "null as Object", null, (null as Object));

// as RegExp
AddTestCase( "null as RegExp", null, (null as RegExp));

// as String
AddTestCase( "null as String", null, (null as String));
AddTestCase( "undefined as String", null, (undefined as String));
AddTestCase( "'' as String", "undefined", ("undefined" as String));
AddTestCase( "'foo' as String", "foo", ('foo' as String));
AddTestCase( "new String('foo') as String", "foo", (new String('foo') as String));
AddTestCase( "Array as String", null, (Array as String));
AddTestCase( "Boolean as String", null, (Boolean as String));
AddTestCase( "new Boolean(true) as String", null, (new Boolean(true) as String));
AddTestCase( "Date as String", null, (Date as String));
AddTestCase( "Function as String", null, (Function as String));
AddTestCase( "Math as String", null, (Math as String));
AddTestCase( "Number as String", null, (Number as String));
AddTestCase( "Object as String", null, (Object as String));
AddTestCase( "RegExp as String", null, (RegExp as String));
AddTestCase( "int as String", null, (int as String));
AddTestCase( "uint as String", null, (uint as String));
//AddTestCase( "void as String", "[class void]", (void as String));

// as int
AddTestCase( "null as int", null, (null as int));
AddTestCase( "0 as int", 0, (0 as int));
AddTestCase( "1 as int", 1, (1 as int));
AddTestCase( "-1 as int", -1, (-1 as int));

// as uint
AddTestCase( "null as uint", null, (null as uint));
AddTestCase( "0 as uint", 0, (0 as uint));
AddTestCase( "1 as uint", 1, (1 as uint));
AddTestCase( "100 as uint", 100, (100 as uint));
AddTestCase( "-1 as uint", null, (-1 as uint));

// as void
//AddTestCase( "null as void", undefined, (null as void));


test();       // leave this alone.  this executes the test cases and
              // displays results.


