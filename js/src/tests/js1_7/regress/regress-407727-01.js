/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 407727;
var summary = 'let Object global redeclaration';
var actual = '';
var expect = 1;

printBugNumber(BUGNUMBER);
printStatus (summary);
 
let Object = 1;
actual = Object;
reportCompare(expect, actual, summary);
