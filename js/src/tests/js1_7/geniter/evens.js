/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "Array comprehensions evens example from http://developer.mozilla.org/en/docs/New_in_JavaScript_1.7"
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function range(begin, end) {
  for (let i = begin; i < end; ++i) {
    yield i;
  }
}
var evens = [i for (i in range(0, 21)) if (i % 2 == 0)];

reportCompare("object", typeof evens, summary);
reportCompare("0,2,4,6,8,10,12,14,16,18,20", "" + evens, summary);
