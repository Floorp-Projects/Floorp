/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.
* All Rights Reserved.
*
* Contributor(s): chwu@nortelnetworks.com, pschwartau@netscape.com
* Date: 10 October 2001
*
* SUMMARY: Regression test for Bugzilla bug 104077
* See http://bugzilla.mozilla.org/show_bug.cgi?id=104077
* 
* SpiderMonkey crashed on this code -
*/
//-----------------------------------------------------------------------------
var UBound = 0;
var bug = 104077;
var summary = "Just testing that we don't crash on this code -";
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


function addValues(obj)
{
  var sum;
  with (obj)
  {
    try
    {
      sum = arg1 + arg2;
    }
    finally
    {
      return sum;
    }
  }
}


status = inSection(1);
var obj = new Object();
obj.arg1 = 1;
obj.arg2 = 2;
actual = addValues(obj);
expect = 3;
captureThis();



function tryThis()
{
  var sum = 4 ;
  var i = 0;

  while (sum < 10)
  {
    try
    {
     sum += 1;
     i += 1;
    }
    finally
    {
     print("In finally case of tryThis() function");
    }
  }
  return i;
}


status = inSection(2);
actual = tryThis();
expect = 6;
captureThis();



function myTest(x)
{
  var obj = new Object();
  var msg;

  with (obj)
  {
    msg = (x != null) ? "NO" : "YES";
    print("Is the provided argument to myTest() null? : " + msg);

    try
    {
      throw "ZZZ";
    }
    catch (e)
    {
      print("Caught thrown exception = " + e);
    }
  }

  return 1;
}


status = inSection(3);
actual = myTest(null);
expect = 1;
captureThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function captureThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
