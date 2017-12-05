/* -*- tab-width: 8; indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


test();

function test()
{
  var EXCEPTION_DATA = "String exception";
  var e = "foo", x = "foo";
  var caught = false;

  printStatus ("Catchguard 'Common Scope' test.");
   
  try
  {   
    throw EXCEPTION_DATA;  
  }
  catch (e if ((x = 1) && false))
  {
    reportCompare('PASS', 'FAIL',
		  "Catch block (e if ((x = 1) && false) should not " +
		  "have executed.");
  }
  catch (e if (x == 1))
  {  
    caught = true;
  }
  catch (e)
  {  
    reportCompare('PASS', 'FAIL',
		  "Same scope should be used across all catchguards.");
  }

  if (!caught)
    reportCompare('PASS', 'FAIL',
		  "Exception was never caught.");
   
  if (e != "foo")
    reportCompare('PASS', 'FAIL',
		  "Exception data modified inside catch() scope should " +
		  "not be visible in the function scope (e ='" +
		  e + "'.)");

  if (x != 1)
    reportCompare('PASS', 'FAIL',
		  "Data modified in 'catchguard expression' should " +
		  "be visible in the function scope (x = '" +
		  x + "'.)");

  reportCompare('PASS', 'PASS', 'Catchguard Common Scope test');
}
