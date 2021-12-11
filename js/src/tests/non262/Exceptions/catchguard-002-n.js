/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


DESCRIPTION = "var in catch clause should have caused an error.";

var expect;
var actual;

test();

function test()
{
  var EXCEPTION_DATA = "String exception";
  var e;

  printStatus ("Catchguard var declaration negative test.");
   
  try
  {   
    throw EXCEPTION_DATA;  
  }
  catch (var e)
  {  
    actual = e + '';
  }

  reportCompare(expect, actual, DESCRIPTION);
}
