/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 358594;
var summary = 'Do not crash on uneval(this).';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

// don't crash|assert
f = function () { };
f.__proto__ = this; 
Object.defineProperty(this, "m", { set: f, enumerable: true, configurable: true });
uneval(this);
reportCompare(expect, actual, summary);
