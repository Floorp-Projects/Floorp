/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    21 February 2003
 * SUMMARY: Testing eval statements containing conditional function expressions
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=194364
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 194364;
var summary = 'Testing eval statements with conditional function expressions';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

/*
  From ECMA-262 Edition 3, 12.4:

  12.4 Expression Statement

  Syntax
  ExpressionStatement : [lookahead not in {{, function}] Expression ;

  Note that an ExpressionStatement cannot start with an opening curly brace
  because that might make it ambiguous with a Block. Also, an ExpressionStatement
  cannot start with the function keyword because that might make it ambiguous with
  a FunctionDeclaration.
*/

status = inSection(1);
actual = eval('(function() {}); 1');
expect = 1;
addThis();

status = inSection(2);
actual = eval('(function f() {}); 2');
expect = 2;
addThis();

status = inSection(3);
actual = eval('if (true) (function() {}); 3');
expect = 3;
addThis();

status = inSection(4);
actual = eval('if (true) (function f() {}); 4');
expect = 4;
addThis();

status = inSection(5);
actual = eval('if (false) (function() {}); 5');
expect = 5;
addThis();

status = inSection(6);
actual = eval('if (false) (function f() {}); 6');
expect = 6;
addThis();

status = inSection(7);
actual = eval('switch(true) { case true: (function() {}) }; 7');
expect = 7;
addThis();

status = inSection(8);
actual = eval('switch(true) { case true: (function f() {}) }; 8');
expect = 8;
addThis();

status = inSection(9);
actual = eval('switch(false) { case false: (function() {}) }; 9');
expect = 9;
addThis();

status = inSection(10);
actual = eval('switch(false) { case false: (function f() {}) }; 10');
expect = 10;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
