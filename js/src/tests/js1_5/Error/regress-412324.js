/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 412324;
var summary = 'Allow function Error(){} for the love of Pete';
var actual = 'No Error';
var expect = 'No Error';

printBugNumber(BUGNUMBER);
printStatus (summary);

function Error() {}

reportCompare(expect, actual, summary);
