/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 427196;
var summary = 'Do not assert: OBJ_SCOPE(pobj)->object == pobj';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function boom()
  {
    var c = {__proto__: []};
    var a = {__proto__: {__proto__: {}}};
    c.hasOwnProperty("t");
    c.__proto__ = a;
    c.hasOwnProperty("v");
  }

  boom();

  reportCompare(expect, actual, summary);
}
