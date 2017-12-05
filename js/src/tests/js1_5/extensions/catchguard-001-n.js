/* -*- tab-width: 8; indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


DESCRIPTION = " the non-guarded catch should HAVE to appear last";

test();

function test()
{
  var EXCEPTION_DATA = "String exception";
  var e;

  printStatus ("Catchguard syntax negative test.");
   
  try
  {   
    throw EXCEPTION_DATA;  
  }
  catch (e) /* the non-guarded catch should HAVE to appear last */
  {  

  }
  catch (e if true)
  {

  }
  catch (e if false)
  {  

  }

  reportCompare('PASS', 'FAIL',
		"Illegally constructed catchguard should have thrown " +
		"an exception.");
}
