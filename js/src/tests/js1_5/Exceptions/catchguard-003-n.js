/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


DESCRIPTION = "Illegally constructed catchguard should have thrown an exception.";
EXPECTED = "error";

var expect;
var actual;

test();

function test()
{
  enterFunc ("test");

  var EXCEPTION_DATA = "String exception";
  var e;

  printStatus ("Catchguard syntax negative test #2.");
   
  try
  {   
    throw EXCEPTION_DATA;  
  }
  catch (e)
  {  
    actual = e + ': 1';
  }
  catch (e) /* two non-guarded catch statements should generate an error */
  {
    actual = e + ': 2';
  }

  reportCompare(expect, actual, DESCRIPTION);

  exitFunc ("test");
}
