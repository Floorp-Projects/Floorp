// |reftest| skip-if(!this.uneval)

/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 304897;
var summary = 'uneval("\\t"), uneval("\\x09")';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = '"\\t"';
actual = uneval('\t'); 
reportCompare(expect, actual, summary);

actual = uneval('\x09'); 
reportCompare(expect, actual, summary);
