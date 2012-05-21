// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349605;
var summary = 'decompilation of let inside |for| statements';
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

  var f = function()
    { alert(1); for((let(y=3) let(y=4) y); 0; x++) ; alert(6); }

  expect = 'function () {\n    alert(1);\n' +
    '    for ((let (y = 3) (let (y = 4) y)); 0; x++) {\n' +
    '    }\n' +
    '    alert(6);\n' +
    '}';

  actual = f + '';

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
