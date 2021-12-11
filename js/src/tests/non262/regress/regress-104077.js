/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date: 10 October 2001
 * SUMMARY: Regression test for Bugzilla bug 104077
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=104077
 * "JS crash: with/finally/return"
 *
 * Also http://bugzilla.mozilla.org/show_bug.cgi?id=120571
 * "JS crash: try/catch/continue."
 *
 * SpiderMonkey crashed on this code - it shouldn't.
 *
 * NOTE: the finally-blocks below should execute even if their try-blocks
 * have return or throw statements in them:
 *
 * ------- Additional Comment #76 From Mike Shaver 2001-12-07 01:21 -------
 * finally trumps return, and all other control-flow constructs that cause
 * program execution to jump out of the try block: throw, break, etc.  Once you
 * enter a try block, you will execute the finally block after leaving the try,
 * regardless of what happens to make you leave the try.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 104077;
var summary = "Just testing that we don't crash on with/finally/return -";
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
    catch(e)
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



function addValues_2(obj)
{
  var sum = 0;
  with (obj)
  {
    try
    {
      sum = arg1 + arg2;
      with (arg3)
      {
        while (sum < 10)
        {
          try
          {
            if (sum > 5)
              return sum;
            sum += 1;
          }
          catch(e)
          {
            print('Caught an exception in addValues_2() function: ' + e);
          }
        }
      }
    }
    finally
    {
      return sum;
    }
  }
}

status = inSection(4);
obj = new Object();
obj.arg1 = 1;
obj.arg2 = 2;
obj.arg3 = new Object();
obj.arg3.a = 10;
obj.arg3.b = 20;
actual = addValues_2(obj);
expect = 6;
captureThis();



status = inSection(5);
try
{
  throw new A();
}
catch(e)
{
}
finally
{
  try
  {
    throw new A();
  }
  catch(e)
  {
  }
  finally
  {
    actual = 'a';
  }
  actual = 'b';
}
expect = 'b';
captureThis();




function testfunc(mode)
{
  var obj = new Object();
  with (obj)
  {
    var num = 100;
    var str = "abc" ;

    if (str == null)
    {
      try
      {
        throw "authentication.0";
      }
      catch(e)
      {
      }
      finally
      {
      }

      return num;
    }
    else
    {
      try
      {
        if (mode == 0)
          throw "authentication.0";
        else
          mytest();
      }
      catch(e)
      {
      }
      finally
      {
      }

      return num;
    }
  }
}

status = inSection(6);
actual = testfunc(0);
expect = 100;
captureThis();

status = inSection(7);
actual = testfunc();
expect = 100; 
captureThis();




function entry_menu()
{
  var document = new Object();
  var dialog = new Object();
  var num = 100;

  with (document)
  {
    with (dialog)
    {
      try
      {
        while (true)
        {
          return num;
        }
      }
      finally
      {
      }
    }
  }
}

status = inSection(8);
actual = entry_menu();
expect = 100;
captureThis();




function addValues_5(obj)
{
  var sum = 0;

  with (obj)
  {
    try
    {
      sum = arg1 + arg2;
      with (arg3)
      {
        while (sum < 10)
        {
          try
          {
	    if (sum > 5)
	      return sum;
	    sum += 1;
          }
          catch (e)
          {
            sum += 1;
          }
        }
      }
    }
    finally
    {
      try
      {
        sum += 1;
        print("In finally block of addValues_5() function: sum = " + sum);
      }
      catch (e)
      {
        sum += 1;
        print("In finally catch block of addValues_5() function: sum = " + sum + ", e = " + e);
      }
      finally
      {
        sum += 1;
        print("In finally finally block of addValues_5() function: sum = " + sum);
        return sum;
      }
    }
  }
}

status = inSection(11);
obj = new Object();
obj.arg1 = 1;
obj.arg2 = 2;
obj.arg3 = new Object();
obj.arg3.a = 10;
obj.arg3.b = 20;
actual = addValues_5(obj);
expect = 8;
captureThis();




function testObj(obj)
{
  var x = 42;

  try
  {
    with (obj)
    {
      if (obj.p)
        throw obj.p;
      x = obj.q;
    }
  }
  finally
  {
    print("in finally block of testObj() function");
    return 999;
  }
}

status = inSection(12);
obj = {p:43};
actual = testObj(obj);
expect = 999;
captureThis();



/*
 * Next two cases are from http://bugzilla.mozilla.org/show_bug.cgi?id=120571
 */
function a120571()
{
  while(0)
  {
    try
    {
    }
    catch(e)
    {
      continue;
    }
  }
}

// this caused a crash! Test to see that it doesn't now.
print(a120571);

// Now test that we have a non-null value for a120571.toString()
status = inSection(13);
try
{
  actual = a120571.toString().match(/continue/)[0];
}
catch(e)
{
  actual = 'FAILED! Did not find "continue" in function body';
}
expect = 'continue';
captureThis();




function b()
{
  for(;;)
  {
    try
    {
    }
    catch(e)
    {
      continue;
    }
  }
}

// this caused a crash!!! Test to see that it doesn't now.
print(b);

// Now test that we have a non-null value for b.toString()
status = inSection(14);
try
{
  actual = b.toString().match(/continue/)[0];
}
catch(e)
{
  actual = 'FAILED! Did not find "continue" in function body';
}
expect = 'continue';
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
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}
