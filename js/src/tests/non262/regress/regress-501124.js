/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 501124;
var summary = 'Crypotographic login routines';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  var hexVal = "00000000000000000000000000000000DEADBABE";
  var nblk   = (((hexVal.length/2) + 8) >> 6) + 1;

  var blks = new Array(nblk * 16);

  for(var i = 0; i < nblk * 16; i++)
    blks[i] = 0;

  for(i = 0; i < hexVal.length; i++) {
    blks[i >> 3] |= ("0x"+hexVal.charAt(i)) << (28 - (i % 8) * 4);
  }

  expect = '0,0,0,0,-559039810,0,0,0,0,0,0,0,0,0,0,0';
  actual   = blks + '';


  reportCompare(expect, actual, summary);
}
