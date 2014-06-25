/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    16 Dec 2002
 * SUMMARY: Testing |with (x) {function f() {}}| when |x.f| already exists
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=185485
 *
 * The idea is this: if |x| does not already have a property named |f|,
 * a |with| statement cannot be used to define one. See, for example,
 *
 *       http://bugzilla.mozilla.org/show_bug.cgi?id=159849#c11
 *       http://bugzilla.mozilla.org/show_bug.cgi?id=184107
 *
 *
 * However, if |x| already has a property |f|, a |with| statement can be
 * used to modify the value it contains:
 *
 *                 with (x) {f = 1;}
 *
 * This should work even if we use a |var| statement, like this:
 *
 *                 with (x) {var f = 1;}
 *
 * However, it should NOT work if we use a |function| statement, like this:
 *
 *                 with (x) {function f() {}}
 *
 * Instead, this should newly define a function f in global scope.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=185485
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 185485;
var summary = 'Testing |with (x) {function f() {}}| when |x.f| already exists';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

var x = { f:0, g:0 };

with (x)
{
  f = 1;
}
status = inSection(1);
actual = x.f;
expect = 1;
addThis();

with (x)
{
  var f = 2;
}
status = inSection(2);
actual = x.f;
expect = 2;
addThis();

/*
 * Use of a function statement under the with-block should not affect
 * the local property |f|, but define a function |f| in global scope -
 */
with (x)
{
  function f() {}
}
status = inSection(3);
actual = x.f;
expect = 2;
addThis();

status = inSection(4);
actual = typeof this.f;
expect = 'function';
addThis();


/*
 * Compare use of function expression instead of function statement.
 * Note it is important that |x.g| already exists. Otherwise, this
 * would newly define |g| in global scope -
 */
with (x)
{
  var g = function() {}
}
status = inSection(5);
actual = x.g.toString();
expect = (function () {}).toString();
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
