/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function boom()
  {
    var b = {};
    Array.__proto__ = b;
    b.__proto__ = {};
    var c = {};
    c.__proto__ = [];
    try { c.__defineSetter__("t", undefined); } catch(e) { }
    c.__proto__ = Array;
    try { c.__defineSetter__("v", undefined); } catch(e) { }
  }

  boom();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
