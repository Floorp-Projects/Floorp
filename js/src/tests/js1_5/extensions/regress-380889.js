/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 380889;
var summary = 'Source disassembler assumes SRC_SWITCH has jump table';
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
 
  function f(i)
  {
    switch(i){
    case 1:
    case xyzzy:
    }
  }

  if (typeof dis != 'undefined')
  {
    dis(f);
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
