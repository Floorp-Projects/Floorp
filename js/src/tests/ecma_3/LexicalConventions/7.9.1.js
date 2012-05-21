/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 402386;
var summary = 'Automatic Semicolon insertion in postfix expressions';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var expr;
  var code;

  // LeftHandSideExpression [no LineTerminator here] ++  

  code   = 'expr ++';
  expr   = 0;
  expect = 1;

  try
  {
    eval(code);
    actual = expr;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': ' + code);

  code   = 'expr\n++';
  expr   = 0;
  expect = 'SyntaxError: syntax error';

  try
  {
    eval(code);
    actual = expr;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': ' + code);

  // LeftHandSideExpression [no LineTerminator here] --

  code   = 'expr --';
  expr   = 0;
  expect = -1;

  try
  {
    eval(code);
    actual = expr;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': ' + code);

  code   = 'expr\n--';
  expr   = 0;
  expect = 'SyntaxError: syntax error';

  try
  {
    eval(code);
    actual = expr;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': ' + code);

  //

  var x = 1;
  var y = 1;
  code   = '(x\n)-- y';
  expect = 'SyntaxError: missing ; before statement';

  try
  {
    eval(code);
    actual = expr;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': ' + code);

  code   = '(x)-- y';
  expect = 'SyntaxError: missing ; before statement';

  try
  {
    eval(code);
    actual = expr;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': ' + code);

  exitFunc ('test');
}

