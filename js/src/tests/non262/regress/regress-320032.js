/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 320032;
var summary = 'Parenthesization should not dereference ECMA Reference type';
var actual = 'No error';
var expect = 'No error';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof document != 'undefined' && 'getElementById' in document)
{
  try
  {
    (document.getElementById)("x");
  }
  catch(ex)
  {
    actual = ex + '';
  }
}
 
reportCompare(expect, actual, summary);
