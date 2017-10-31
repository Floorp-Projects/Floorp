// |reftest| skip-if(xulRuntime.XPCOMABI.match(/x86_64|aarch64|ppc64|ppc64le|s390x/)) -- On 64-bit, takes forever rather than throwing
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 422348;
var summary = 'Proper overflow error reporting';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = 'InternalError: allocation size overflow';
  try 
  { 
    Array(1 << 30).sort(); 
    actual = 'No Error';
  } 
  catch (ex) 
  { 
    actual = ex + '';
  } 

  reportCompare(expect, actual, summary);
}
