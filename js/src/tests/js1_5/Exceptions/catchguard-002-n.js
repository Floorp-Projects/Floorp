/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


DESCRIPTION = "var in catch clause should have caused an error.";
EXPECTED = "error";

var expect;
var actual;

test();

function test()
{
  enterFunc ("test");

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

  exitFunc ("test");
}
