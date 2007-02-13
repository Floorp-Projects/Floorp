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

var SECTION = "ErrorObjects";       // provide a document reference (ie, Actionscript section)
var VERSION = "ES3";        // Version of ECMAScript or ActionScript 
var TITLE   = "new Error(message)";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

var testcases = getTestCases();


test();       // leave this alone.  this executes the test cases and
              // displays results.
              
function getTestCases() {
    var array = new Array();
    var item = 0;              
    var error = new Error();
    var ee = new EvalError();
    var te = new TypeError();
    var re = new ReferenceError();
    var ra = new RangeError();

    array[item++] = new TestCase(SECTION, "var error = new Error()", "Error", error.toString() );
    array[item++] = new TestCase(SECTION, "var ee = new EvalError()", "EvalError", ee.toString() );
    array[item++] = new TestCase(SECTION, "var te = new TypeError()", "TypeError", te.toString() );
    array[item++] = new TestCase(SECTION, "var re = new ReferenceError()", "ReferenceError", re.toString() );
    array[item++] = new TestCase(SECTION, "var ra = new RangeError()", "RangeError", ra.toString() );

    error = new Error("test");
    ee = new EvalError("eval error");
    te = new TypeError("type error");
    re = new ReferenceError("reference error");
    ra = new RangeError("range error");

    array[item++] = new TestCase(SECTION, "var error = new Error('test')", "Error: test", error.toString() );
    array[item++] = new TestCase(SECTION, "var ee = new EvalError('eval error')", "EvalError: eval error", ee.toString() );
    array[item++] = new TestCase(SECTION, "var te = new TypeError('type error')", "TypeError: type error", te.toString() );
    array[item++] = new TestCase(SECTION, "var re = new ReferenceError('reference error')", "ReferenceError: reference error", re.toString() );
    array[item++] = new TestCase(SECTION, "var ra = new RangeError('range error')", "RangeError: range error", ra.toString() );

    array[item++] = new TestCase(SECTION, "typeof new Error()", "object", typeof new Error() );
    array[item++] = new TestCase(SECTION, "typeof new EvalError()", "object", typeof new EvalError() );
    array[item++] = new TestCase(SECTION, "typeof new TypeError()", "object", typeof new TypeError() );
    array[item++] = new TestCase(SECTION, "typeof new ReferenceError()", "object", typeof new ReferenceError() );
    array[item++] = new TestCase(SECTION, "typeof new RangeError()", "object", typeof new RangeError() );

    array[item++] = new TestCase(SECTION, "typeof new Error('test')", "object", typeof new Error('test') );
    array[item++] = new TestCase(SECTION, "typeof new EvalError('test')", "object", typeof new EvalError('test') );
    array[item++] = new TestCase(SECTION, "typeof new TypeError('test')", "object", typeof new TypeError('test') );
    array[item++] = new TestCase(SECTION, "typeof new ReferenceError('test')", "object", typeof new ReferenceError('test') );
    array[item++] = new TestCase(SECTION, "typeof new RangeError('test')", "object", typeof new RangeError('test') );

    array[item++] = new TestCase(SECTION, "(err = new Error(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object Error]",
    			 (err = new Error(), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = new EvalError(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object EvalError]",
    			 (err = new EvalError(), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = new TypeError(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object TypeError]",
    			 (err = new TypeError(), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = new ReferenceError(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object ReferenceError]",
    			 (err = new ReferenceError(), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = new RangeError(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object RangeError]",
    			 (err = new RangeError(), err.getClass = Object.prototype.toString, err.getClass() ) );

    array[item++] = new TestCase(SECTION, "(err = new Error('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object Error]",
    			 (err = new Error('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = new EvalError('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object EvalError]",
    			 (err = new EvalError('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = new TypeError('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object TypeError]",
    			 (err = new TypeError('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = new ReferenceError('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object ReferenceError]",
    			 (err = new ReferenceError('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = new RangeError('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object RangeError]",
    			 (err = new RangeError('test'), err.getClass = Object.prototype.toString, err.getClass() ) );

    return array;
}
