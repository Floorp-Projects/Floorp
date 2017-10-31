/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    05 June 2003
 * SUMMARY: Testing |with (f)| inside the definition of |function f()|
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=208496
 *
 * In this test, we check that static function properties of
 * of |f| are read correctly from within the |with(f)| block.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 208496;
var summary = 'Testing |with (f)| inside the definition of |function f()|';
var STATIC_VALUE = 'read the static property';
var status = '';
var statusitems = [];
var actual = '(TEST FAILURE)';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


function f(par)
{
  with(f)
  {
    actual = par;
  }

  return par;
}
f.par = STATIC_VALUE;


status = inSection(1);
f('abc'); // this sets |actual| inside |f|
expect = STATIC_VALUE;
addThis();

// test the return: should be the dynamic value
status = inSection(2);
actual = f('abc');
expect = 'abc';
addThis();

status = inSection(3);
f(111 + 222); // sets |actual| inside |f|
expect = STATIC_VALUE;
addThis();

// test the return: should be the dynamic value
status = inSection(4);
actual = f(111 + 222);
expect = 333;
addThis();


/*
 * Add a level of indirection via |x|
 */
function g(par)
{
  with(g)
  {
    var x = par;
    actual = x;
  }

  return par;
}
g.par = STATIC_VALUE;


status = inSection(5);
g('abc'); // this sets |actual| inside |g|
expect = STATIC_VALUE;
addThis();

// test the return: should be the dynamic value
status = inSection(6);
actual = g('abc');
expect = 'abc';
addThis();

status = inSection(7);
g(111 + 222); // sets |actual| inside |g|
expect = STATIC_VALUE;
addThis();

// test the return: should be the dynamic value
status = inSection(8);
actual = g(111 + 222);
expect = 333;
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
