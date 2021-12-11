/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    07 February 2003
 * SUMMARY: Testing 0/0 inside functions
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=192288
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 192288;
var summary = 'Testing 0/0 inside functions ';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


function f()
{
  return 0/0;
}

status = inSection(1);
actual = isNaN(f());
expect = true;
addThis();

status = inSection(2);
actual = isNaN(f.apply(this));
expect = true;
addThis();

status = inSection(3);
actual = isNaN(eval("f.apply(this)"));
expect = true;
addThis();

status = inSection(4);
actual = isNaN(Function('return 0/0;')());
expect = true;
addThis();

status = inSection(5);
actual = isNaN(eval("Function('return 0/0;')()"));
expect = true;
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
