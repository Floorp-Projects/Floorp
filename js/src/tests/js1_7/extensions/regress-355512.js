/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 355512;
var summary = 'Do not crash with generator arguments';
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
 
  function foopy()
  {
    var f = function(){ r = arguments; d.d.d; yield 170; }
    try { for (var i in f()) { } } catch (iterError) { }  
  }

  typeof uneval;
  foopy();
  gc();
  uneval(r);
  gc();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
