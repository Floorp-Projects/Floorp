/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352640;
var summary = 'Do not assert: scopeStmt or crash @ js_LexicalLookup';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  try
  {
    new Function("switch(w) { case 2: with({}) let y; case 3: }");
  }
  catch(ex)
  {
  }
  reportCompare(expect, actual, summary);
}
