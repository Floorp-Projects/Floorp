/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          while-002
 *  ECMA Section:
 *  Description:        while statement
 *
 *  Verify that the while statement is not executed if the while expression is
 *  false
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "while-002";
var TITLE   = "while statement";

writeHeaderToLog( SECTION + " "+ TITLE);

DoWhile( new DoWhileObject(
	   "while expression is null",
	   null,
	   "result = \"fail: should not have evaluated statements in while block;break"
	   ) );

DoWhile( new DoWhileObject(
	   "while expression is undefined",
	   void 0,
	   "result = \"fail: should not have evaluated statements in while block; break"
	   ));

DoWhile( new DoWhileObject(
	   "while expression is 0",
	   0,
	   "result = \"fail: should not have evaluated statements in while block; break;"
	   ));

DoWhile( new DoWhileObject(
	   "while expression is eval(\"\")",
	   eval(""),
	   "result = \"fail: should not have evaluated statements in while block; break"
	   ));

DoWhile( new DoWhileObject(
	   "while expression is NaN",
	   NaN,
	   "result = \"fail: should not have evaluated statements in while block; break"
	   ));

test();

function DoWhileObject( d, e, s ) {
  this.description = d;
  this.whileExpression = e;
  this.statements = s;
}

function DoWhile( object ) {
  result = "pass";

  while ( expression = object.whileExpression ) {
    eval( object.statements );
  }

  // verify that the while expression was evaluated

  new TestCase(
    "verify that while expression was evaluated (should be "+
    object.whileExpression +")",
    "pass",
    (object.whileExpression == expression ||
     ( isNaN(object.whileExpression) && isNaN(expression) )
      ) ? "pass" : "fail" );

  new TestCase(
    object.description,
    "pass",
    result );
}
