/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 320172;
var summary = 'Regression from bug 285219';
var actual = 'No Crash';
var expect = 'No Crash';

enterFunc ('test');
printBugNumber(BUGNUMBER);
printStatus (summary);

try
{
  (function xxx(){ ["var x"].forEach(eval); })();
}
catch(ex)
{
}

printStatus('No Crash');
reportCompare(expect, actual, summary);
