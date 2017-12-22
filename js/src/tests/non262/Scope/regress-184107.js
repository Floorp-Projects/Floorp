/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    09 December 2002
 * SUMMARY: with(...) { function f ...} should set f in the global scope
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=184107
 *
 * In fact, any variable defined in a with-block should be created
 * in global scope, i.e. should be a property of the global object.
 *
 * The with-block syntax allows existing local variables to be SET,
 * but does not allow new local variables to be CREATED.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=159849#c11
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 184107;
var summary = 'with(...) { function f ...} should set f in the global scope';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

var obj = {y:10};
with (obj)
{
  // function statement
  function f()
  {
    return y;
  }

  // function expression
  g = function() {return y;}
}

status = inSection(1);
actual = obj.f;
expect = undefined;
addThis();

status = inSection(2);
actual = f();
expect = obj.y;
addThis();

status = inSection(3);
actual = obj.g;
expect = undefined;
addThis();

status = inSection(4);
actual = g();
expect = obj.y;
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
