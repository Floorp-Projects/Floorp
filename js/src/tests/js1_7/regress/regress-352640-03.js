/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  try
  {
    new Function("do { with({}) let x; [2 for each (b in [])]; } while( 1 );");
  }
  catch(ex)
  {
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
