/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 375882;
var summary = 'Decompilation of switch with case 0/0';
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
    {
      switch(a)
      {
        case 0/0: a;
        case 1/0: b;
        case -1/0: c;
        case -0:
        d;
      }
    };

  expect = 'function ( ) { switch ( a ) { case 0 / 0 : a ; case 1 / 0 : b ; ' +
    'case 1 / - 0 : c ; case - 0 : d ; default : ; } } ';

  actual = f + '';

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
