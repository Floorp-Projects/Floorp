/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "364104";
var summary = "Array.prototype.indexOf, Array.prototype.lastIndexOf issues " +
  "with the optional second fromIndex argument";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

try
{
  // indexOf
  if ([2].indexOf(2) != 0)
    throw "indexOf: not finding 2!?";
  if ([2].indexOf(2, 0) != 0)
    throw "indexOf: not interpreting explicit second argument 0!";
  if ([2].indexOf(2, 1) != -1)
    throw "indexOf: ignoring second argument with value equal to array length!";
  if ([2].indexOf(2, 2) != -1)
    throw "indexOf: ignoring second argument greater than array length!";
  if ([2].indexOf(2, 17) != -1)
    throw "indexOf: ignoring large second argument!";
  if ([2].indexOf(2, -5) != 0)
    throw "indexOf: calculated fromIndex < 0, should search entire array!";
  if ([2, 3].indexOf(2, -1) != -1)
    throw "indexOf: not handling index == (-1 + 2), element 2 correctly!";
  if ([2, 3].indexOf(3, -1) != 1)
    throw "indexOf: not handling index == (-1 + 2), element 3 correctly!";

  // lastIndexOf
  if ([2].lastIndexOf(2) != 0)
    throw "lastIndexOf: not finding 2!?";
  if ([2].lastIndexOf(2, 1) != 0)
    throw "lastIndexOf: not interpreting explicit second argument 1!?";
  if ([2].lastIndexOf(2, 17) != 0)
    throw "lastIndexOf: should have searched entire array!";
  if ([2].lastIndexOf(2, -5) != -1)
    throw "lastIndexOf: -5 + 1 < 0, so array shouldn't be searched!";
  if ([2].lastIndexOf(2, -2) != -1)
    throw "lastIndexOf: -2 + 1 < 0, so array shouldn't be searched!";
  if ([2, 3].lastIndexOf(2, -1) != 0)
    throw "lastIndexOf: not handling index == (-1 + 2), element 2 correctly!";
  if ([2, 3].lastIndexOf(3, -1) != 1)
    throw "lastIndexOf: not handling index == (-1 + 2), element 3 correctly!";
  if ([2, 3].lastIndexOf(2, -2) != 0)
    throw "lastIndexOf: not handling index == (-2 + 2), element 2 correctly!";
  if ([2, 3].lastIndexOf(3, -2) != -1)
    throw "lastIndexOf: not handling index == (-2 + 2), element 3 correctly!";
  if ([2, 3].lastIndexOf(2, -3) != -1)
    throw "lastIndexOf: calculated fromIndex < 0, shouldn't search array for 2!";
  if ([2, 3].lastIndexOf(3, -3) != -1)
    throw "lastIndexOf: calculated fromIndex < 0, shouldn't search array for 3!";
}
catch (e)
{
  failed = e;
}


expect = false;
actual = failed;

reportCompare(expect, actual, summary);
