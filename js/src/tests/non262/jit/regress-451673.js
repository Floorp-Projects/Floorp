// |reftest| skip -- bogus perf test (bug 540512)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 451673;
var summary = 'TM: Tracing prime number generation';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function doTest(enablejit)
  {
    if (enablejit)
    else

    var n = 1000000;
    var start = new Date();
    var i=0;
    var j=0;
    var numprimes=0;
    var limit=0;
    numprimes = 1; // 2 is prime
    var mceil = Math.floor;
    var msqrt = Math.sqrt;
    var isPrime = 1;

    for (i = 3; i<= n; i+=2)
    {
	    isPrime=1;
	    limit = mceil(msqrt(i)+1) + 1;

	    for (j = 3; j < limit; j+=2)
      {
		    if (i % j == 0)
        {
			    isPrime = 0;
			    break;
        }
      }

	    if (isPrime)
      {
		    numprimes ++;
      }
    }

    var end = new Date();

    var timetaken = end - start;
    timetaken = timetaken / 1000;

    if (enablejit)

    print((enablejit ? '    JIT' : 'Non-JIT') + ": Number of primes up to: " + n + " is " + numprimes + ", counted in " + timetaken + " secs.");

    return timetaken;
  }

  var timenonjit = doTest(false);
  var timejit    = doTest(true);

  expect = true;
  actual = timejit < timenonjit;

  reportCompare(expect, actual, summary);
}
