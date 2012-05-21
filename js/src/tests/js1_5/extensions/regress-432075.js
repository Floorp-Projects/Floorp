// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 432075;
var summary = 'A function decompiles as [object Function] after export/import';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var a = Function;
var t = this;
export *;
import t.*;
Function = a;

expect = 'function anonymous() {}';
actual = (new Function("")) + '';

compareSource(expect, actual, summary);
