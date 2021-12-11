/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 469239;
var summary = 'TM: Do not assert: entry->kpc == (jsbytecode*) atoms[index]';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  for (let b=0;b<9;++b) {
    for (let h of ['', 3, /x/]) {
	    for (c of [[], [], [], /x/]) {
        '' + c;
	    }
    }
  }


  reportCompare(expect, actual, summary);
}
