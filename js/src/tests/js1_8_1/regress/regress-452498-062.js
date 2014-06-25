/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
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

// ------- Comment #62 From Gary Kwong [:nth10sd]

  try
  {
    eval(
      '(function(){' +
      '  var x;' +
      '  this.init_by_array = function()' +
      '    x = 0;' +
      '})();'
      );
  }
  catch(ex)
  {
  }

// Assertion failure: ATOM_IS_STRING(atom), at ../jsinterp.cpp:5686

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
