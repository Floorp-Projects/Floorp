/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 416460;
var summary = 'Do not assert: SCOPE_GET_PROPERTY(OBJ_SCOPE(pobj), ATOM_TO_JSID(atom))';
var actual = 'No Crash';
var expect = 'No Crash';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  /a/.__proto__.__proto__ = { "2": 3 };
  var b = /b/;
  b["2"];
  b["2"];

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
