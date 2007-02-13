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



import ExtError.*;

var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "AS 3.0";  // Version of JavaScript or ECMA
var TITLE   = "extend Error";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


var error = new CustError();
var ee = new CustEvalError();
var te = new CustTypeError();
var re = new CustReferenceError();
var ra = new CustRangeError();

AddTestCase( "var error = new CustError()", "Error", error.toString() );
AddTestCase( "var ee = new CustEvalError()", "EvalError", ee.toString() );
AddTestCase( "var te = new CustTypeError()", "TypeError", te.toString() );
AddTestCase( "var re = new CustReferenceError()", "ReferenceError", re.toString() );
AddTestCase( "var ra = new CustRangeError()", "RangeError", ra.toString() );

AddTestCase( "typeof new CustError()", "object", typeof new CustError() );
AddTestCase( "typeof new CustEvalError()", "object", typeof new CustEvalError() );
AddTestCase( "typeof new CustTypeError()", "object", typeof new CustTypeError() );
AddTestCase( "typeof new CustReferenceError()", "object", typeof new CustReferenceError() );
AddTestCase( "typeof new CustRangeError()", "object", typeof new CustRangeError() );


// dynamic Cust Error Cases

error = new CustError2();
ee = new CustEvalError2();
te = new CustTypeError2();
re = new CustReferenceError2();
ra = new CustRangeError2();

AddTestCase( "var error = new CustError2()", "Error", error.toString() );
AddTestCase( "var ee = new CustEvalError2()", "EvalError", ee.toString() );
AddTestCase( "var te = new CustTypeError2()", "TypeError", te.toString() );
AddTestCase( "var re = new CustReferenceError2()", "ReferenceError", re.toString() );
AddTestCase( "var ra = new CustRangeError2()", "RangeError", ra.toString() );

error = new CustError2("test");
ee = new CustEvalError2("eval error");
te = new CustTypeError2("type error");
re = new CustReferenceError2("reference error");
ra = new CustRangeError2("range error");

AddTestCase( "var error = new CustError2('test')", "Error: test", error.toString() );
AddTestCase( "var ee = new CustEvalError2('eval error')", "EvalError: eval error", ee.toString() );
AddTestCase( "var te = new CustTypeError2('type error')", "TypeError: type error", te.toString() );
AddTestCase( "var re = new CustReferenceError2('reference error')", "ReferenceError: reference error", re.toString() );
AddTestCase( "var ra = new CustRangeError2('range error')", "RangeError: range error", ra.toString() );

AddTestCase( "typeof new CustError2()", "object", typeof new CustError2() );
AddTestCase( "typeof new CustEvalError2()", "object", typeof new CustEvalError2() );
AddTestCase( "typeof new CustTypeError2()", "object", typeof new CustTypeError2() );
AddTestCase( "typeof new CustReferenceError2()", "object", typeof new CustReferenceError2() );
AddTestCase( "typeof new CustRangeError2()", "object", typeof new CustRangeError2() );

AddTestCase( "typeof new CustError2('test')", "object", typeof new CustError2('test') );
AddTestCase( "typeof new CustEvalError2('test')", "object", typeof new CustEvalError2('test') );
AddTestCase( "typeof new CustTypeError2('test')", "object", typeof new CustTypeError2('test') );
AddTestCase( "typeof new CustReferenceError2('test')", "object", typeof new CustReferenceError2('test') );
AddTestCase( "typeof new CustRangeError2('test')", "object", typeof new CustRangeError2('test') );


AddTestCase( "(err = new CustError2(), err.getClass = Object.prototype.toString, err.getClass() )",
			 "[object CustError2]",
			 (err = new CustError2(), err.getClass = Object.prototype.toString, err.getClass() ) );
AddTestCase( "(err = new CustEvalError2(), err.getClass = Object.prototype.toString, err.getClass() )",
			 "[object CustEvalError2]",
			 (err = new CustEvalError2(), err.getClass = Object.prototype.toString, err.getClass() ) );
AddTestCase( "(err = new CustTypeError2(), err.getClass = Object.prototype.toString, err.getClass() )",
			 "[object CustTypeError2]",
			 (err = new CustTypeError2(), err.getClass = Object.prototype.toString, err.getClass() ) );
AddTestCase( "(err = new CustReferenceError2(), err.getClass = Object.prototype.toString, err.getClass() )",
			 "[object CustReferenceError2]",
			 (err = new CustReferenceError2(), err.getClass = Object.prototype.toString, err.getClass() ) );
AddTestCase( "(err = new CustRangeError2(), err.getClass = Object.prototype.toString, err.getClass() )",
			 "[object CustRangeError2]",
			 (err = new CustRangeError2(), err.getClass = Object.prototype.toString, err.getClass() ) );

AddTestCase( "(err = new CustError2('test'), err.getClass = Object.prototype.toString, err.getClass() )",
			 "[object CustError2]",
			 (err = new CustError2('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
AddTestCase( "(err = new CustEvalError2('test'), err.getClass = Object.prototype.toString, err.getClass() )",
			 "[object CustEvalError2]",
			 (err = new CustEvalError2('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
AddTestCase( "(err = new CustTypeError2('test'), err.getClass = Object.prototype.toString, err.getClass() )",
			 "[object CustTypeError2]",
			 (err = new CustTypeError2('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
AddTestCase( "(err = new CustReferenceError2('test'), err.getClass = Object.prototype.toString, err.getClass() )",
			 "[object CustReferenceError2]",
			 (err = new CustReferenceError2('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
AddTestCase( "(err = new CustRangeError2('test'), err.getClass = Object.prototype.toString, err.getClass() )",
			 "[object CustRangeError2]",
			 (err = new CustRangeError2('test'), err.getClass = Object.prototype.toString, err.getClass() ) );



test();       // leave this alone.  this executes the test cases and
              // displays results.
