/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351794;
var summary = 'Do not assert: CG_NOTE_COUNT(cg) == 0 || ' +
  'CG_LAST_NOTE_OFFSET(cg) != CG_OFFSET(cg)';
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
 
  new Function("for(let y in [5,6,7,8]) x") 
    reportCompare(expect, actual, summary);

  var f = function f(){for(let y in [5,6,7,8]) x}
  actual = f + '';
  expect = 'function f() {\n    for (let y in [5, 6, 7, 8]) {\n' +
    '        x;\n    }\n}';
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
