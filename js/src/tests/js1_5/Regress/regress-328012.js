/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 328012;
var summary = 'Content PropertyIterator should not root in chrome';
var actual = 'No Error';
var expect = 'No Error';


printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof focus != 'undefined' && focus.prototype)
{
  try
  {
    for (prop in focus.prototype.toString)
      ;
  }
  catch(ex)
  {
    printStatus(ex + '');
    actual = ex + '';
  }
}
reportCompare(expect, actual, summary);
