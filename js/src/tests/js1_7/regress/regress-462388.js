/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 462388;
var summary = 'Do not assert: JSVAL_TAG(v) == JSVAL_STRING';
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
 

  var c = 0, v; for each (let x in ["",v,v,v]) { for (c=0;c<4;++c) { } }


  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
