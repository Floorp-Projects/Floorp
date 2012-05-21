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

printBugNumber(BUGNUMBER);
printStatus (summary);

status = inSection(1) + ' /((\3|b)\2(a)x)+/.exec("aaxabxbaxbbx") ';

actual = 'undefined'; 
expect = ['ax', 'ax', '', 'a'] + '';

try
{ 
  actual = /((\3|b)\2(a)x)+/.exec("aaxabxbaxbbx") + '';
}
catch(e)
{
  status += ' Error: ' + e;
}

reportCompare(expect, actual, status);

status = inSection(2) + ' eval(\'/((\3|b)\2(a)x)+/.exec("aaxabxbaxbbx")\' ';

actual = 'undefined'; 
expect = ['ax', 'ax', '', 'a'] + '';

try
{ 
  actual = eval('/((\\3|b)\\2(a)x)+/.exec("aaxabxbaxbbx")') + '';
}
catch(e)
{
  status += ' Error: ' + e;
}

reportCompare(expect, actual, status);
