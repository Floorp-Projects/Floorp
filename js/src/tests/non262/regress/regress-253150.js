/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 253150;
var summary = 'Do not warn on detecting properties';
var actual = '';
var expect = 'No warning';

printBugNumber(BUGNUMBER);
printStatus (summary);

var testobject = {};

options('strict');
options('werror');

try
{
  var testresult = testobject.foo;
  actual = 'No warning';
}
catch(ex)
{
  actual = ex + '';
}

reportCompare(expect, actual, summary + ': 1');

try
{
  if (testobject.foo)
  {
    ;
  }
  actual = 'No warning';
}
catch(ex)
{
  actual = ex + '';
}

reportCompare(expect, actual, summary + ': 2');

try
{
  if (typeof testobject.foo == 'undefined')
  {
    ;
  }
  actual = 'No warning';
}
catch(ex)
{
  actual = ex + '';
}

reportCompare(expect, actual, summary + ': 3');

try
{
  if (testobject.foo == null)
  {
    ;
  }
  actual = 'No warning';
}
catch(ex)
{
  actual = ex + '';
}

reportCompare(expect, actual, summary + ': 4');

try
{
  if (testobject.foo == undefined)
  {
    ;
  }
  actual = 'No warning';
}
catch(ex)
{
  actual = ex + '';
}

reportCompare(expect, actual, summary + ': 3');
