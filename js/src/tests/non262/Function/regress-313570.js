/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 313570;
var summary = 'length of objects whose prototype chain includes a function';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function tmp() {}
tmp.prototype = function(a, b, c) {};
var obj = new tmp();

// arity
expect = 3;
actual = obj.length;
reportCompare(expect, actual, summary + ': arity');

// immutable
obj.length = 10;

expect = 3;
actual = obj.length;
reportCompare(expect, actual, summary + ': immutable');

