/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465132;
var summary = 'TM: Mathematical constants should be constant';
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


  var constants = ['E', 'LN10', 'LN2', 'LOG2E', 'LOG10E', 'PI', 'SQRT1_2', 'SQRT2'];

  for (var j = 0; j < constants.length; j++)
  {
    expect = Math[constants[j]];

    for(i=0;i<9;++i) 
      ++Math[constants[j]];

    for(i=0;i<9;++i) 
      eval('++Math.' + constants[j]);

    actual = Math[constants[j]];

    reportCompare(expect, actual, summary + ' Math.' + constants[j]);
  }


  exitFunc ('test');
}
