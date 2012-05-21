/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
// testcase by Martin Zvieger <martin.zvieger@sphinx.at>
// if fails, will fail to run in browser due to syntax error
var BUGNUMBER = 275378;
var summary = 'Literal RegExp in case block should not give syntax error';
var actual = '';
var expect = '';

var status;

printBugNumber(BUGNUMBER);
printStatus (summary);


var tmpString= "XYZ";
// works
/ABC/.test(tmpString);
var tmpVal= 1;
if (tmpVal == 1)
{
  // works
  /ABC/.test(tmpString);
}
switch(tmpVal)
{
case 1:
{
  // works
  /ABC/.test(tmpString);
}
break;
}
switch(tmpVal)
{
case 1:
  // fails with syntax error
  /ABC/.test(tmpString);
  break;
}

expect = actual = 'no error';
reportCompare(expect, actual, status);
