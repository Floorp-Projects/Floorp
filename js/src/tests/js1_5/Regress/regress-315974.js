/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------

/*
  This test case uses String.quote(), because that method uses the JS printf()
  functions internally. The printf() functions print to a char* buffer, causing
  conversion to and from UTF-8 if UTF-8 is enabled. If not, UTF-8 sequences are
  converted to ASCII \uXXXX sequences. Thus, both return values are acceptable.
*/

var BUGNUMBER = 315974;
var summary = 'Test internal printf() for wide characters';

printBugNumber(BUGNUMBER);
printStatus (summary);

enterFunc (String (BUGNUMBER));

var goodSurrogatePair = '\uD841\uDC42';
var badSurrogatePair = '\uD841badbad';

var goodSurrogatePairQuotedUtf8 = '"\uD841\uDC42"';
var badSurrogatePairQuotedUtf8 = 'no error thrown!';
var goodSurrogatePairQuotedNoUtf8 = '"\\uD841\\uDC42"';
var badSurrogatePairQuotedNoUtf8 = '"\\uD841badbad"';

var status = summary + ': String.quote() should pay respect to surrogate pairs';
var actual = goodSurrogatePair.quote();
/* Result in case UTF-8 is enabled. */
var expect = goodSurrogatePairQuotedUtf8;
/* Result in case UTF-8 is disabled. */
if (actual != expect && actual == goodSurrogatePairQuotedNoUtf8)
  expect = actual;
reportCompare(expect, actual, status);

/*
 * A malformed surrogate pair throws an out-of-memory error,
 * but only if UTF-8 is enabled.
 */
status = summary + ': String.quote() should throw error on bad surrogate pair';
/* Out of memory is not catchable. */
actual = badSurrogatePair.quote();
/* Come here only if UTF-8 is disabled. */
reportCompare(badSurrogatePairQuotedNoUtf8, actual, status);

exitFunc (String (BUGNUMBER));
