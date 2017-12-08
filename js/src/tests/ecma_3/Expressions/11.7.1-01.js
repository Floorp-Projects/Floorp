/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 396969;
var summary = '11.7.1 - << should evaluate operands in order';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = 'o.valueOf, p.valueOf';
  var actualval;
  var expectval = 168;

  var o = {
    valueOf: (function (){ actual += 'o.valueOf'; return this.value}), 
    value:42
  };

  var p = {
    valueOf: (function (){ actual += ', p.valueOf'; return this.value}), 
    value:2
  };

  actualval = (o << p);

  reportCompare(expectval, actualval, summary + ': value');
  reportCompare(expect, actual, summary + ': order');
}
