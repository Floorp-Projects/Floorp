// |reftest| skip-if(!xulRuntime.shell) slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 319980;
var summary = 'GC not called during non-fatal out of memory';
var actual = '';
var expect = 'Normal Exit';

printBugNumber(BUGNUMBER);
printStatus (summary);
print ('This test should never fail explicitly. ' +
       'You must view the memory usage during the test. ' +
       'This test fails if memory usage for each subtest grows');

var timeOut  = 45 * 1000;
var interval = 0.01  * 1000;
var testFuncWatcherId;
var testFuncTimerId;
var maxTests = 5;
var currTest = 0;

if (typeof setTimeout == 'undefined')
{
  setTimeout = function() {};
  clearTimeout = function() {};
  actual = 'Normal Exit';
  reportCompare(expect, actual, summary);
}
else
{
  // delay start until after js-test-driver-end runs.
  // delay test driver end
  gDelayTestDriverEnd = true;

  setTimeout(testFuncWatcher, 1000);
}

function testFuncWatcher()
{
  a = null;

  gc();

  clearTimeout(testFuncTimerId);
  testFuncWatcherId = testFuncTimerId = null;
  if (currTest >= maxTests)
  {
    actual = 'Normal Exit';
    reportCompare(expect, actual, summary);
    printStatus('Test Completed');
    gDelayTestDriverEnd = false;
    jsTestDriverEnd();
    return;
  }
  ++currTest;
 
  print('Executing test ' + currTest + '\n');

  testFuncWatcherId = setTimeout("testFuncWatcher()", timeOut);
  testFuncTimerId = setTimeout(testFunc, interval);
}


var a;
function testFunc()
{

  var i;

  switch(currTest)
  {
  case 1:
    a = new Array(100000);
    for (i = 0; i < 100000; i++ )
    {
      a[i] = i;
    }
    break;

  case 2:
    a = new Array(100000);
    for (i = 0; i < 100000; i++)
    {
      a[i] = new Number();
      a[i] = i;
    }
    break;

  case 3:
    a = new String() ;
    a = new Array(100000);
    for ( i = 0; i < 100000; i++ )
    {
      a[i] = i;
    }

    break;

  case 4:
    a = new Array();
    a[0] = new Array(100000);
    for (i = 0; i < 100000; i++ )
    {
      a[0][i] = i;
    }
    break;

  case 5:
    a = new Array();
    for (i = 0; i < 100000; i++ )
    {
      a[i] = i;
    }
    break;
  }

  if (testFuncTimerId)
  {
    testFuncTimerId = setTimeout(testFunc, interval);
  }
}


