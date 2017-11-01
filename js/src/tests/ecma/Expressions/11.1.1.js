/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.1.1.js
   ECMA Section:       11.1.1 The this keyword
   Description:

   The this keyword evaluates to the this value of the execution context.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.1.1";

writeHeaderToLog( SECTION + " The this keyword");

var GLOBAL_OBJECT = this.toString();

// this in global code and eval(this) in global code should return the global object.

new TestCase(
	      "Global Code: this.toString()",
	      GLOBAL_OBJECT,
	      this.toString() );

new TestCase(
	      "Global Code:  eval('this.toString()')",
	      GLOBAL_OBJECT,
	      eval('this.toString()') );

// this in anonymous code called as a function should return the global object.

new TestCase(
	      "Anonymous Code: var MYFUNC = new Function('return this.toString()'); MYFUNC()",
	      GLOBAL_OBJECT,
	      eval("var MYFUNC = new Function('return this.toString()'); MYFUNC()") );

// eval( this ) in anonymous code called as a function should return that function's activation object

new TestCase(
	      "Anonymous Code: var MYFUNC = new Function('return (eval(\"this.toString()\")'); (MYFUNC()).toString()",
	      GLOBAL_OBJECT,
	      eval("var MYFUNC = new Function('return eval(\"this.toString()\")'); (MYFUNC()).toString()") );

// this and eval( this ) in anonymous code called as a constructor should return the object

new TestCase(
	      "Anonymous Code: var MYFUNC = new Function('this.THIS = this'); ((new MYFUNC()).THIS).toString()",
	      "[object Object]",
	      eval("var MYFUNC = new Function('this.THIS = this'); ((new MYFUNC()).THIS).toString()") );

new TestCase(
	      "Anonymous Code: var MYFUNC = new Function('this.THIS = this'); var FUN1 = new MYFUNC(); FUN1.THIS == FUN1",
	      true,
	      eval("var MYFUNC = new Function('this.THIS = this'); var FUN1 = new MYFUNC(); FUN1.THIS == FUN1") );

new TestCase(
	      "Anonymous Code: var MYFUNC = new Function('this.THIS = eval(\"this\")'); ((new MYFUNC().THIS).toString()",
	      "[object Object]",
	      eval("var MYFUNC = new Function('this.THIS = eval(\"this\")'); ((new MYFUNC()).THIS).toString()") );

new TestCase(
	      "Anonymous Code: var MYFUNC = new Function('this.THIS = eval(\"this\")'); var FUN1 = new MYFUNC(); FUN1.THIS == FUN1",
	      true,
	      eval("var MYFUNC = new Function('this.THIS = eval(\"this\")'); var FUN1 = new MYFUNC(); FUN1.THIS == FUN1") );

// this and eval(this) in function code called as a function should return the global object.
new TestCase(
	      "Function Code:  ReturnThis()",
	      GLOBAL_OBJECT,
	      ReturnThis() );

new TestCase(
	      "Function Code:  ReturnEvalThis()",
	      GLOBAL_OBJECT,
	      ReturnEvalThis() );

//  this and eval(this) in function code called as a contructor should return the object.
new TestCase(
	      "var MYOBJECT = new ReturnThis(); MYOBJECT.toString()",
	      "[object Object]",
	      eval("var MYOBJECT = new ReturnThis(); MYOBJECT.toString()") );

new TestCase(
	      "var MYOBJECT = new ReturnEvalThis(); MYOBJECT.toString()",
	      "[object Object]",
	      eval("var MYOBJECT = new ReturnEvalThis(); MYOBJECT.toString()") );

test();

function ReturnThis() {
  return this.toString();
}

function ReturnEvalThis() {
  return( eval("this.toString()") );
}
