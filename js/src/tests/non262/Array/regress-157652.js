// |reftest| skip-if(xulRuntime.XPCOMABI.match(/x86_64|aarch64|ppc64|ppc64le|s390x|mips64/)||Android) -- No test results
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    16 July 2002
 * SUMMARY: Testing that Array.sort() doesn't crash on very large arrays
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=157652
 *
 * How large can a JavaScript array be?
 * ECMA-262 Ed.3 Final, Section 15.4.2.2 : new Array(len)
 *
 * This states that |len| must be a a uint32_t (unsigned 32-bit integer).
 * Note the UBound for uint32's is 2^32 -1 = 0xFFFFFFFF = 4,294,967,295.
 *
 * Check:
 *              js> var arr = new Array(0xFFFFFFFF)
 *              js> arr.length
 *              4294967295
 *
 *              js> var arr = new Array(0x100000000)
 *              RangeError: invalid array length
 *
 *
 * We'll try the largest possible array first, then a couple others.
 * We're just testing that we don't crash on Array.sort().
 *
 * Try to be good about memory by nulling each array variable after it is
 * used. This will tell the garbage collector the memory is no longer needed.
 *
 * As of 2002-08-13, the JS shell runs out of memory no matter what we do,
 * when trying to sort such large arrays.
 *
 * We only want to test that we don't CRASH on the sort. So it will be OK
 * if we get the JS "out of memory" error. Note this terminates the test
 * with exit code 3. Therefore we put
 *
 *                       |expectExitCode(3);|
 *
 * The only problem will arise if the JS shell ever DOES have enough memory
 * to do the sort. Then this test will terminate with the normal exit code 0
 * and fail.
 *
 * Right now, I can't see any other way to do this, because "out of memory"
 * is not a catchable error: it cannot be trapped with try...catch.
 *
 *
 * FURTHER HEADACHE: Rhino can't seem to handle the largest array: it hangs.
 * So we skip this case in Rhino. Here is correspondence with Igor Bukanov.
 * He explains that Rhino isn't actually hanging; it's doing the huge sort:
 *
 * Philip Schwartau wrote:
 *
 * > Hi,
 * >
 * > I'm getting a graceful OOM message on trying to sort certain large
 * > arrays. But if the array is too big, Rhino simply hangs. Note that ECMA
 * > allows array lengths to be anything less than Math.pow(2,32), so the
 * > arrays I'm sorting are legal.
 * >
 * > Note below, I'm getting an instantaneous OOM error on arr.sort() for LEN
 * > = Math.pow(2, 30). So shouldn't I also get one for every LEN between
 * > that and Math.pow(2, 32)? For some reason, I start to hang with 100% CPU
 * > as LEN hits, say, Math.pow(2, 31) and higher. SpiderMonkey gives OOM
 * > messages for all of these. Should I file a bug on this?
 *
 *   Igor Bukanov wrote:
 *
 * This is due to different sorting algorithm Rhino uses when sorting
 * arrays with length > Integer.MAX_VALUE. If length can fit Java int,
 * Rhino first copies internal spare array to a temporary buffer, and then
 * sorts it, otherwise it sorts array directly. In case of very spare
 * arrays, that Array(big_number) generates, it is rather inefficient and
 * generates OutOfMemory if length fits int. It may be worth in your case
 * to optimize sorting to take into account array spareness, but then it
 * would be a good idea to file a bug about ineficient sorting of spare
 * arrays both in case of Rhino and SpiderMonkey as SM always uses a
 * temporary buffer.
 *
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 157652;
var summary = "Testing that Array.sort() doesn't crash on very large arrays";
var expect = 'No Crash';
var actual = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus(summary);

expectExitCode(0);
expectExitCode(5);

try
{
  var a1=Array(0xFFFFFFFF);
  a1.sort();
  a1 = null;

  var a2 = Array(0x40000000);
  a2.sort();
  a2=null;

  var a3=Array(0x10000000/4);
  a3.sort();
  a3=null;
}
catch(ex)
{
  // handle changed 1.9 branch behavior. see bug 422348
  expect = 'InternalError: allocation size overflow';
  actual = ex + '';
}

reportCompare(expect, actual, summary);
