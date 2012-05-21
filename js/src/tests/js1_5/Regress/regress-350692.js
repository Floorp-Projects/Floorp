// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350692;
var summary = 'import x["y"]["z"]';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var x = {y: {z: function() {}}};

  try
  {
    import x['y']['z'];
  }
  catch(ex)
  {
    reportCompare('TypeError: x["y"]["z"] is not exported', ex + '', summary);
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
