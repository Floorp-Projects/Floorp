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
var TITLE   = "The Error Constructor Called as a Function";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

var testcases = getTestCases();

test();       // leave this alone.  this executes the test cases and
              // displays results.
              
function getTestCases() {
    var array = new Array();
    var item = 0;
    
    var error = Error();
    var ee = EvalError();
    var te = TypeError();
    var re = ReferenceError();
    var ra = RangeError();
    var SE = SyntaxError();
    var URI = URIError();

    array[item++] = new TestCase(SECTION, "var error = Error()", "Error", error.toString() );
    array[item++] = new TestCase(SECTION, "var ee = EvalError()", "EvalError", ee.toString() );
    array[item++] = new TestCase(SECTION, "var te = TypeError()", "TypeError", te.toString() );
    array[item++] = new TestCase(SECTION, "var re = ReferenceError()", "ReferenceError", re.toString() );
    array[item++] = new TestCase(SECTION, "var ra = RangeError()", "RangeError", ra.toString() );
    array[item++] = new TestCase(SECTION, "var SE = SyntaxError()", "SyntaxError", SE.toString() );
    array[item++] = new TestCase(SECTION,"var URI = URIError()", "URIError", URI.toString() );

    error = Error("test");
    ee = EvalError("eval error");
    te = TypeError("type error");
    re = ReferenceError("reference error");
    ra = RangeError("range error");
    var SE = SyntaxError("syntax error");
    var URI = URIError("uri error");

    array[item++] = new TestCase(SECTION, "var error = Error('test')", "Error: test", error.toString() );
    array[item++] = new TestCase(SECTION, "var ee = EvalError('eval error')", "EvalError: eval error", ee.toString() );
    array[item++] = new TestCase(SECTION, "var te = TypeError('type error')", "TypeError: type error", te.toString() );
    array[item++] = new TestCase(SECTION, "var re = ReferenceError('reference error')", "ReferenceError: reference error", re.toString() );
    array[item++] = new TestCase(SECTION, "var ra = RangeError('range error')", "RangeError: range error", ra.toString() );
    array[item++] = new TestCase(SECTION, "var SE = SyntaxError('syntax error')", "SyntaxError: syntax error", SE.toString() );
    array[item++] = new TestCase(SECTION, "var URI = URIError('uri error')", "URIError: uri error", URI.toString() );
    
    array[item++] = new TestCase(SECTION, "typeof Error()", "object", typeof Error() );
    array[item++] = new TestCase(SECTION, "typeof EvalError()", "object", typeof EvalError() );
    array[item++] = new TestCase(SECTION, "typeof TypeError()", "object", typeof TypeError() );
    array[item++] = new TestCase(SECTION, "typeof ReferenceError()", "object", typeof ReferenceError() );
    array[item++] = new TestCase(SECTION, "typeof RangeError()", "object", typeof RangeError() );
    array[item++] = new TestCase(SECTION, "typeof SyntaxError()", "object", typeof SyntaxError() );
    array[item++] = new TestCase(SECTION, "typeof URIError()", "object", typeof URIError() );
    
    array[item++] = new TestCase(SECTION, "typeof Error('test')", "object", typeof Error('test') );
    array[item++] = new TestCase(SECTION, "typeof EvalError('test')", "object", typeof EvalError('test') );
    array[item++] = new TestCase(SECTION, "typeof TypeError('test')", "object", typeof TypeError('test') );
    array[item++] = new TestCase(SECTION, "typeof ReferenceError('test')", "object", typeof ReferenceError('test') );
    array[item++] = new TestCase(SECTION, "typeof RangeError('test')", "object", typeof RangeError('test') );
    array[item++] = new TestCase(SECTION, "typeof SyntaxError()", "object", typeof SyntaxError('test') );
    array[item++] = new TestCase(SECTION, "typeof URIError()", "object", typeof URIError('test') );
    
    array[item++] = new TestCase(SECTION, "(err = Error(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object Error]",
    			 (err = Error(), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = EvalError(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object EvalError]",
    			 (err = EvalError(), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = TypeError(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object TypeError]",
    			 (err = TypeError(), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = ReferenceError(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object ReferenceError]",
    			 (err = ReferenceError(), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = RangeError(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object RangeError]",
    			 (err = RangeError(), err.getClass = Object.prototype.toString, err.getClass() ) );
    
    array[item++] = new TestCase(SECTION, "(err = SyntaxError(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object SyntaxError]",
    			 (err = SyntaxError(), err.getClass = Object.prototype.toString, err.getClass() ) );
    
    array[item++] = new TestCase(SECTION, "(err = URIError(), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object URIError]",
    			 (err = URIError(), err.getClass = Object.prototype.toString, err.getClass() ) );
    
    array[item++] = new TestCase(SECTION, "(err = Error('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object Error]",
    			 (err = Error('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = EvalError('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object EvalError]",
    			 (err = EvalError('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = TypeError('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object TypeError]",
    			 (err = TypeError('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = ReferenceError('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object ReferenceError]",
    			 (err = ReferenceError('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = RangeError('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object RangeError]",
    			 (err = RangeError('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = SyntaxError('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object SyntaxError]",
    			 (err = SyntaxError('test'), err.getClass = Object.prototype.toString, err.getClass() ) );
    array[item++] = new TestCase(SECTION, "(err = URIError('test'), err.getClass = Object.prototype.toString, err.getClass() )",
    			 "[object URIError]",
    			 (err = URIError('test'), err.getClass = Object.prototype.toString, err.getClass() ) );

    return array;
}

