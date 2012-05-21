/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
EXPECTED = 'error';

printBugNumber(BUGNUMBER);
printStatus (summary);

status = inSection(1) + ' check for overflow in quantifier';

actual = 'undefined'; 
expect = 'error';

try
{
  var result = eval('/a{21474836481}/.test("a")');
  actual = 'no exception thrown';
  status += ' result: ' + result;
}
catch(e)
{
  actual = 'error';
}

reportCompare(expect, actual, status);

