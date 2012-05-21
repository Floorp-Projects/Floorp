// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 340526;
var summary = 'Iterators: cross-referenced objects with close handler can ' +
  'delay close handler execution';

printBugNumber(BUGNUMBER);
printStatus (summary);

var close_count = 0;

function gen()
{
  try {
    yield 0;
  } finally {
    ++close_count;
  }
}

var iter1 = gen();
var iter2 = gen();

iter1.another = iter2;
iter2.another = iter1;

iter1.next();
iter2.next();

iter1 = null;
iter2 = null;

gc();

var expect = 2;
var actual = close_count;

reportCompare(expect, actual, summary);
