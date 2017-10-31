/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    21 May 2002
 * SUMMARY: ECMA conformance of Function.prototype.apply
 *
 *   Function.prototype.apply(thisArg, argArray)
 *
 * See ECMA-262 Edition 3 Final, Section 15.3.4.3
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 145791;
var summary = 'Testing ECMA conformance of Function.prototype.apply';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


function F0(a)
{
  return "" + this + arguments.length;
}

function F1(a)
{
  return "" + this + a;
}

function F2()
{
  return "" + this;
}



/*
 * Function.prototype.apply.length should return 2
 */
status = inSection(1);
actual = Function.prototype.apply.length;
expect = 2;
addThis();


/*
 * When |thisArg| is not provided to the apply() method, the
 * called function must be passed the global object as |this|
 */
status = inSection(2);
actual = F0.apply();
expect = "" + this + 0;
addThis();


/*
 * If |argArray| is not provided to the apply() method, the
 * called function should be invoked with an empty argument list
 */
status = inSection(3);
actual = F0.apply("");
expect = "" + "" + 0;
addThis();

status = inSection(4);
actual = F0.apply(true);
expect = "" + true + 0;
addThis();


/*
 * Function.prototype.apply(x) and
 * Function.prototype.apply(x, undefined) should return the same result
 */
status = inSection(5);
actual = F1.apply(0, undefined);
expect = F1.apply(0);
addThis();

status = inSection(6);
actual = F1.apply("", undefined);
expect = F1.apply("");
addThis();

status = inSection(7);
actual = F1.apply(null, undefined);
expect = F1.apply(null);
addThis();

status = inSection(8);
actual = F1.apply(undefined, undefined);
expect = F1.apply(undefined);
addThis();


/*
 * Function.prototype.apply(x) and
 * Function.prototype.apply(x, null) should return the same result
 */
status = inSection(9);
actual = F1.apply(0, null);
expect = F1.apply(0);
addThis();

status = inSection(10);
actual = F1.apply("", null);
expect = F1.apply("");
addThis();

status = inSection(11);
actual = F1.apply(null, null);
expect = F1.apply(null);
addThis();

status = inSection(12);
actual = F1.apply(undefined, null);
expect = F1.apply(undefined);
addThis();


/*
 * Function.prototype.apply() and
 * Function.prototype.apply(undefined) should return the same result
 */
status = inSection(13);
actual = F2.apply(undefined);
expect = F2.apply();
addThis();


/*
 * Function.prototype.apply() and
 * Function.prototype.apply(null) should return the same result
 */
status = inSection(14);
actual = F2.apply(null);
expect = F2.apply();
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
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}
