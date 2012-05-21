/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    14 Mar 2003
 * SUMMARY: Testing left-associativity of the + operator
 *
 * See ECMA-262 Ed.3, Section 11.6.1, "The Addition operator"
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=196290
 *
 * The upshot: |a + b + c| should always equal |(a + b) + c|
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 196290;
var summary = 'Testing left-associativity of the + operator';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


status = inSection(1);
actual = 1 + 1 + 'px';
expect = '2px';
addThis();

status = inSection(2);
actual = 'px' + 1 + 1;
expect = 'px11';
addThis();

status = inSection(3);
actual = 1 + 1 + 1 + 'px';
expect = '3px';
addThis();

status = inSection(4);
actual = 1 + 1 + 'a' + 1 + 1 + 'b';
expect = '2a11b';
addThis();

/*
 * The next sections test the + operator via eval()
 */
status = inSection(5);
actual = sumThese(1, 1, 'a', 1, 1, 'b');
expect = '2a11b';
addThis();

status = inSection(6);
actual = sumThese(new Number(1), new Number(1), 'a');
expect = '2a';
addThis();

status = inSection(7);
actual = sumThese('a', new Number(1), new Number(1));
expect = 'a11';
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

/*
 * Applies the + operator to the provided arguments via eval().
 *
 * Form an eval string of the form 'arg1 + arg2 + arg3', but
 * remember to add double-quotes inside the eval string around
 * any argument that is of string type. For example, suppose the
 * arguments were 11, 'a', 22. Then the eval string should be
 *
 *              arg1 + quoteThis(arg2) + arg3
 *
 * If we didn't put double-quotes around the string argument,
 * we'd get this for an eval string:
 *
 *                     '11 + a + 22'
 *
 * If we eval() this, we get 'ReferenceError: a is not defined'.
 * With proper quoting, we get eval('11 + "a" + 22') as desired.
 */
function sumThese()
{
  var sEval = '';
  var arg;
  var i;

  var L = arguments.length;
  for (i=0; i<L; i++)
  {
    arg = arguments[i];
    if (typeof arg === 'string')
      arg = quoteThis(arg);

    if (i < L-1)
      sEval += arg + ' + ';
    else
      sEval += arg;
  }

  return eval(sEval);
}


function quoteThis(x)
{
  return '"' + x + '"';
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
