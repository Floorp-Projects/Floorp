/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 406555;
var summary = 'decompiler should not depend on JS_C_STRINGS_ARE_UTF8';
var actual;
var expect;


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var f = (function() {return "\uD800\uD800";});
  var g = uneval(f);
  var h = eval(g);

  expect = "\uD800\uD800";
  actual = h();
  reportCompare(expect, actual, summary + ': h() == \\uD800\\uD800');

  expect = g;
  actual = uneval(h);
  reportCompare(expect, actual, summary + ': g == uneval(h)');

  exitFunc ('test');
}
