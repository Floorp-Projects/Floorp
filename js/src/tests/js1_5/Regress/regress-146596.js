/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    18 Jun 2002
 * SUMMARY: Shouldn't crash when catch parameter is "hidden" by varX
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=146596
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 146596;
var summary = "Shouldn't crash when catch parameter is 'hidden' by varX";
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * Just seeing we don't crash when executing this function -
 * This example provided by jim-patterson@ncf.ca
 *
 * Brendan: "Jim, thanks for the testcase. But note that |var|
 * in a JS function makes a function-scoped variable --  JS lacks
 * block scope apart from for catch variables within catch blocks.
 *
 * Therefore the catch variable hides the function-local variable."
 */
function F()
{
  try
  {
    return "A simple exception";
  }
  catch(e)
  {
    var e = "Another exception";
  }

  return 'XYZ';
}

status = inSection(1);
actual = F();
expect = "A simple exception";
addThis();



/*
 * Sanity check by Brendan: "This should output
 *
 *             24
 *             42
 *          undefined
 *
 * and throw no uncaught exception."
 *
 */
function f(obj)
{
  var res = [];

  try
  {
    throw 42;
  }
  catch(e)
  {
    with(obj)
    {
      var e;
      res[0] = e; // |with| binds tighter than |catch|; s/b |obj.e|
    }

    res[1] = e;   // |catch| binds tighter than function scope; s/b 42
  }

  res[2] = e;     // |var e| has function scope; s/b visible but contain |undefined|
  return res;
}

status = inSection(2);
actual = f({e:24});
expect = [24, 42, undefined];
addThis();




//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual.toString();
  expectedvalues[UBound] = expect.toString();
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
