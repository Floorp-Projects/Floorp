/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 376052;
var summary = 'Unnamed function expressions are forbidden in statement context';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  try
  {
    eval('(function () {1;})');
    reportCompare(true, true,
                  "unnamed function expression not in statement context works");
  }
  catch(ex)
  {
    reportCompare(true, false, "threw exception: " + ex);
  }

  try
  {
    eval('function () {1;}');
    reportCompare(true, false, "didn't throw an exception");
  }
  catch(ex)
  {
    reportCompare(ex instanceof SyntaxError, true,
                  "unnamed function expression not in statement context " +
                  "should have been a SyntaxError");
  }
}
