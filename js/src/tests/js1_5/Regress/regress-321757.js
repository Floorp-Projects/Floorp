/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 321757;
var summary = 'Compound assignment operators should not bind LHS';
var actual = '';
var expect = 'pass';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{
  foo += 1;
  actual = "fail";
}
catch (e)
{
  actual = "pass";
}
 
reportCompare(expect, actual, summary + ': +=');

try
{
  foo -= 1;
  actual = "fail";
}
catch (e)
{
  actual = "pass";
}
 
reportCompare(expect, actual, summary + ': -=');

try
{
  foo *= 1;
  actual = "fail";
}
catch (e)
{
  actual = "pass";
}
 
reportCompare(expect, actual, summary + ': *=');

try
{
  foo /= 1;
  actual = "fail";
}
catch (e)
{
  actual = "pass";
}
 
reportCompare(expect, actual, summary + ': /=');

try
{
  foo %= 1;
  actual = "fail";
}
catch (e)
{
  actual = "pass";
}
 
reportCompare(expect, actual, summary + ': %=');

try
{
  foo <<= 1;
  actual = "fail";
}
catch (e)
{
  actual = "pass";
}
 
reportCompare(expect, actual, summary + ': <<=');

try
{
  foo >>= 1;
  actual = "fail";
}
catch (e)
{
  actual = "pass";
}
 
reportCompare(expect, actual, summary + ': >>=');

try
{
  foo >>>= 1;
  actual = "fail";
}
catch (e)
{
  actual = "pass";
}
 
reportCompare(expect, actual, summary + ': >>>=');
