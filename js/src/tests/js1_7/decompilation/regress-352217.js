/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352217;
var summary = 'Need space between |else|, |let|';
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

  var c;
  var f;

  try
  {
    c = '(function() { if(g) h; else let x; })';
    f = eval(c);
    expect = 'function() { if(g) { h; } else let x; }';
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    summary = 'let declaration must be direct child of block or top-level implicit block';
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summary);
  }

  exitFunc ('test');
}
