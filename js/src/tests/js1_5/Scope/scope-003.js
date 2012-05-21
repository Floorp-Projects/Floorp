/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 2001-07-03
 *
 * SUMMARY:  Testing scope with nested functions
 *
 * From correspondence with Christopher Oliver <coliver@mminternet.com>:
 *
 * > Running this test with Rhino produces the following exception:
 * >
 * > uncaught JavaScript exception: undefined: Cannot find default value for
 * > object. (line 3)
 * >
 * > This is due to a bug in org.mozilla.javascript.NativeCall which doesn't
 * > implement toString or valueOf or override getDefaultValue.
 * > However, even after I hacked in an implementation of getDefaultValue in
 * > NativeCall, Rhino still produces a different result then SpiderMonkey:
 * >
 * > [object Call]
 * > [object Object]
 * > [object Call]
 *
 * Note the results should be:
 *
 *   [object global]
 *   [object Object]
 *   [object global]
 *
 * This is what we are checking for in this testcase -
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing scope with nested functions';
var statprefix = 'Section ';
var statsuffix = ' of test -';
var self = this; // capture a reference to the global object;
var cnGlobal = self.toString();
var cnObject = (new Object).toString();
var statusitems = [];
var actualvalues = [];
var expectedvalues = [];


function a()
{
  function b()
  {
    capture(this.toString());
  }

  this.c = function()
    {
      capture(this.toString());
      b();
    }

  b();
}


var obj = new a();  // captures actualvalues[0]
obj.c();            // captures actualvalues[1], actualvalues[2]


// The values we expect - see introduction above -
expectedvalues[0] = cnGlobal;
expectedvalues[1] = cnObject;
expectedvalues[2] = cnGlobal;



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function capture(val)
{
  actualvalues[UBound] = val;
  statusitems[UBound] = getStatus(UBound);
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}


function getStatus(i)
{
  return statprefix + i + statsuffix;
}
