/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    30 January 2003
 * SUMMARY: Testing |this[name]| via Function.prototype.call(), apply()
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=191276
 *
 * Igor: "This script fails when run in Rhino compiled mode, but passes in
 * interpreted mode. Note that presence of the never-called |unused_function|
 * with |f('a')| line is essential; the script works OK without it."
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 191276;
var summary = 'Testing |this[name]| via Function.prototype.call(), apply()';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


function F(name)
{
  return this[name];
}

function unused_function()
{
  F('a');
}

status = inSection(1);
actual = F.call({a: 'aaa'}, 'a');
expect = 'aaa';
addThis();

status = inSection(2);
actual = F.apply({a: 'aaa'}, ['a']);
expect = 'aaa';
addThis();

/*
 * Try the same things with an object variable instead of a literal
 */
var obj = {a: 'aaa'};

status = inSection(3);
actual = F.call(obj, 'a');
expect = 'aaa';
addThis();

status = inSection(4);
actual = F.apply(obj, ['a']);
expect = 'aaa';
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
