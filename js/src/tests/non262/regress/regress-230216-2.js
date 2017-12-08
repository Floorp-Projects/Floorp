/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 230216;
var summary = 'check for numerical overflow in regexps in back reference and bounds for {} quantifier';
var actual = '';
var expect = '';
var status = '';

DESCRIPTION = summary;

printBugNumber(BUGNUMBER);
printStatus (summary);

status = inSection(1) + ' check for overflow in quantifier';

actual = 'undefined'; 
expect0 = 'no exception thrown false';
expect1 = 'error';

try
{
  var result = eval('/a{21474836481}/.test("a")');
  actual = 'no exception thrown ' + result;
  status += ' result: ' + result;
}
catch(e)
{
  actual = 'error';
}

// The result we get depends on the regexp engine.
if (actual != 'error')
  reportCompare(expect0, actual, status);
else
  reportCompare(expect1, actual, status);
