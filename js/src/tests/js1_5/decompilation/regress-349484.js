// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349484;
var summary = 'Incorrect decompilation of import/export statements';
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
 
  expect = 'function () {\n    export x;\n    alert(5);\n}';
  var f = function() { export x; alert(5); };
  actual = f.toString();
  print(f.toString());
  compareSource(expect, actual, summary);

  expect = 'function () {\n    import o.x;\n    alert(5);\n}';
  f = function () { import o.x; alert(5); }
  actual = f.toString();
  print(f.toString());
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
