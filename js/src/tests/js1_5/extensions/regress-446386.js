/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 446386;
var summary = 'Do not crash throwing error without compiler pseudo-frame';
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

  if (typeof evalcx == 'undefined')
  {
    print(expect = actual = 'Test skipped. evalcx required.');
  }
  else {
    try
    {
      try {
	evalcx(".");
	throw "must throw";
      } catch (e) {
	if (e.name != "SyntaxError")
	  throw e;
      }
    }
    catch(ex)
    {
      actual = ex + '';
    }
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
