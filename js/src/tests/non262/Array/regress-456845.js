/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 456845;
var summary = 'JIT: popArrs[a].pop is not a function';
var actual = 'No Error';
var expect = 'No Error';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  try
  { 
    var chars = '0123456789abcdef';
    var size = 1000;
    var mult = 100;

    var arr = [];
    var lsize = size;
    while (lsize--) { arr.push(chars); }

    var popArrs = [];
    for (var i=0; i<mult; i++) { popArrs.push(arr.slice()); }


    for(var a=0;a<mult;a++) {
      var x; while (x = popArrs[a].pop()) {  }
    }

  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary);
}
