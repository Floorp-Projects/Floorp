/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    11 Feb 2002
 * SUMMARY: Testing functions having duplicate formal parameter names
 *
 * SpiderMonkey was crashing on each case below if the parameters had
 * the same name. But duplicate parameter names are permitted by ECMA;
 * see ECMA-262 3rd Edition Final Section 10.1.3
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing functions having duplicate formal parameter names';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var OBJ = new Object();
var OBJ_TYPE = OBJ.toString();

/*
 * OK, now begin the test. Just checking that we don't crash on these -
 */
function f1(x,x,x,x)
{
  var ret = eval(arguments.toSource());
  return ret.toString();
}
status = inSection(1);
actual = f1(1,2,3,4);
expect = OBJ_TYPE;
addThis();


/*
 * Same thing, but preface |arguments| with the function name
 */
function f2(x,x,x,x)
{
  var ret = eval(f2.arguments.toSource());
  return ret.toString();
}
status = inSection(2);
actual = f2(1,2,3,4);
expect = OBJ_TYPE;
addThis();


function f3(x,x,x,x)
{
  var ret = eval(uneval(arguments));
  return ret.toString();
}
status = inSection(3);
actual = f3(1,2,3,4);
expect = OBJ_TYPE;
addThis();


/*
 * Same thing, but preface |arguments| with the function name
 */
function f4(x,x,x,x)
{
  var ret = eval(uneval(f4.arguments));
  return ret.toString();
}
status = inSection(4);
actual = f4(1,2,3,4);
expect = OBJ_TYPE;
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
