// |reftest| skip-if(xulRuntime.OS=="WINNT"&&isDebugBuild) slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 314401;
var summary = 'setTimeout(eval,0,"",null)|setTimeout(Script,0,"",null) should not crash';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof setTimeout == 'undefined')
{
  reportCompare(expect, actual, 'Test Skipped.');
}
else
{
  gDelayTestDriverEnd = true;
  window.onerror = null;

  try
  {
    setTimeout(eval, 0, '', null);
  }
  catch(ex)
  {
    printStatus(ex+'');
  }

  reportCompare(expect, actual, 'setTimeout(eval, 0, "", null)');

  if (typeof Script != 'undefined')
  {
    try
    {
      setTimeout(Script, 0, '', null);
    }
    catch(ex)
    {
      printStatus(ex+'');
    }
    reportCompare(expect, actual, 'setTimeout(Script, 0, "", null)');
  }

  try
  {
    setInterval(eval, 0, '', null);
  }
  catch(ex)
  {
    printStatus(ex+'');
  }
  reportCompare(expect, actual, 'setInterval(eval, 0, "", null)');

  if (typeof Script != 'undefined')
  {
    try
    {
      setInterval(Script, 0, '', null);
    }
    catch(ex)
    {
      printStatus(ex+'');
    } 
    reportCompare(expect, actual, 'setInterval(Script, 0, "", null)');
  }
  setTimeout('gDelayTestDriverEnd = false; jsTestDriverEnd();', 0);
}
