/* ***** BEGIN LICENSE BLOCK *****
* Version: NPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Netscape Public License
* Version 1.1 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is JavaScript Engine testing utilities.
*
* The Initial Developer of the Original Code is Netscape Communications Corp.
* Portions created by the Initial Developer are Copyright (C) 2001
* the Initial Developer. All Rights Reserved.
*
* Contributor(s): chwu@nortelnetworks.com, timeless@mac.com,
*                 pschwartau@netscape.com
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the NPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the NPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK *****
*
*
* Date: 10 October 2001
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
            // doing something else
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
