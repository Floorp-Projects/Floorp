// |reftest| skip -- Yarr doesn't have the same complexity errors at execution time.
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351448;
var summary = 'RegExp - throw InternalError on too complex regular expressions';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var strings = [
    "/.X(.+)+X/.exec('bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.X(.+)+X/.exec('bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.X(.+)+XX/.exec('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.X(.+)+XX/.exec('bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.X(.+)+[X]/.exec('bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.X(.+)+[X]/.exec('bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.X(.+)+[X][X]/.exec('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.X(.+)+[X][X]/.exec('bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.XX(.+)+X/.exec('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.XX(.+)+X/.exec('bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.XX(.+)+X/.exec('bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.XX(.+)+[X]/.exec('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.XX(.+)+[X]/.exec('bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.[X](.+)+[X]/.exec('bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.[X](.+)+[X]/.exec('bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.[X](.+)+[X][X]/.exec('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.[X](.+)+[X][X]/.exec('bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.[X][X](.+)+[X]/.exec('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')",
    "/.[X][X](.+)+[X]/.exec('bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')"
    ];

  expect = 'InternalError: regular expression too complex';

  if (!options().match(/relimit/)) {
    options('relimit');
  }

  for (var i = 0; i < strings.length; i++)
  {
    try
    {
      eval(strings[i]);
    }
    catch(ex)
    {
      actual = ex + '';
    }
    reportCompare(expect, actual, summary + ': ' + strings[i]);
  }

  exitFunc ('test');
}
