/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------

var BUGNUMBER = 232182;
var summary = 'Display non-ascii characters in JS exceptions';
var actual = '';
var expect = 'no error';

printBugNumber(BUGNUMBER);
printStatus (summary);

/*
 * This test accesses an undefined Unicode symbol. If the code has not been
 * compiled with JS_C_STRINGS_ARE_UTF8, the thrown error truncates Unicode
 * characters to bytes. Accessing \u0440\u0441, therefore, results in a
 * message talking about an undefined variable 'AB' (\x41\x42).
 */
var utf8Enabled = false;
try
{
  \u0441\u0442;
}
catch (e)
{
  utf8Enabled = (e.message.charAt (0) == '\u0441');
}

// Run the tests only if UTF-8 is enabled

printStatus('UTF-8 is ' + (utf8Enabled?'':'not ') + 'enabled');

if (!utf8Enabled)
{
  reportCompare('Not run', 'Not run', 'utf8 is not enabled');
}
else
{
  status = summary + ': Throw Error with Unicode message';
  expect = 'test \u0440\u0441';
  try
  {
    throw Error (expect);
  }
  catch (e)
  {
    actual = e.message;
  }
  reportCompare(expect, actual, status);

  var inShell = (typeof stringsAreUTF8 == "function");
  if (!inShell)
  {
    inShell = (typeof stringsAreUtf8  == "function");
    if (inShell)
    {
      this.stringsAreUTF8 = stringsAreUtf8;
      this.testUTF8 = testUtf8;
    }
  }

  if (inShell && stringsAreUTF8())
  {
    status = summary + ': UTF-8 test: bad UTF-08 sequence';
    expect = 'Error';
    actual = 'No error!';
    try
    {
      testUTF8(1);
    }
    catch (e)
    {
      actual = 'Error';
    }
    reportCompare(expect, actual, status);

    status = summary + ': UTF-8 character too big to fit into Unicode surrogate pairs';
    expect = 'Error';
    actual = 'No error!';
    try
    {
      testUTF8(2);
    }
    catch (e)
    {
      actual = 'Error';
    }
    reportCompare(expect, actual, status);

    status = summary + ': bad Unicode surrogate character';
    expect = 'Error';
    actual = 'No error!';
    try
    {
      testUTF8(3);
    }
    catch (e)
    {
      actual = 'Error';
    }
    reportCompare(expect, actual, status);
  }

  if (inShell)
  {
    status = summary + ': conversion target buffer overrun';
    expect = 'Error';
    actual = 'No error!';
    try
    {
      testUTF8(4);
    }
    catch (e)
    {
      actual = 'Error';
    }
    reportCompare(expect, actual, status);
  }
}
