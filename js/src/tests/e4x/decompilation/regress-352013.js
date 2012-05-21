/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 352013;
var summary = 'Decompilation with new operator redeaux';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var l, m, r;
var nTests = 0;

// ---------------------------------------------------------------

l = function () { (new x(y))[z]; };
expect = 'function () { (new x(y))[z]; }';
actual = l + '';
compareSource(expect, actual, inSection(++nTests) + summary);

m = function () { new (x(y))[z]; };
expect = 'function () { new (x(y)[z]); }';
actual = m + '';
compareSource(expect, actual, inSection(++nTests) + summary);

r = function () { new (x(y)[z]); };
expect = 'function () { new (x(y)[z]); }';
actual = r + '';
compareSource(expect, actual, inSection(++nTests) + summary);

// ---------------------------------------------------------------

l = function () { (new x(y)).@a; };
expect = 'function () { (new x(y)).@a; }';
actual = l + '';
compareSource(expect, actual, inSection(++nTests) + summary);

m = function () { new (x(y)).@a; };
expect = 'function () { new (x(y).@a); }';
actual = m + '';
compareSource(expect, actual, inSection(++nTests) + summary);

r = function () { new (x(y).@a); };
expect = 'function () { new (x(y).@a); }';
actual = r + '';
compareSource(expect, actual, inSection(++nTests) + summary);

// ---------------------------------------------------------------

l = function () { (new x(y)).@n::a; };
expect = 'function () { (new x(y)).@[n::a]; }';
actual = l + '';
compareSource(expect, actual, inSection(++nTests) + summary);

m = function () { new (x(y)).@n::a; };
expect = 'function () { new (x(y).@[n::a]); }';
actual = m + '';
compareSource(expect, actual, inSection(++nTests) + summary);

r = function () { new (x(y).@n::a); };
expect = 'function () { new (x(y).@[n::a]); }';
actual = r + '';
compareSource(expect, actual, inSection(++nTests) + summary);

// ---------------------------------------------------------------

l = function () { (new x(y)).n::z; };
expect = 'function () { (new x(y)).n::z; }';
actual = l + '';
compareSource(expect, actual, inSection(++nTests) + summary);

m = function () { new (x(y)).n::z; };
expect = 'function () { new (x(y).n::z); }';
actual = m + '';
compareSource(expect, actual, inSection(++nTests) + summary);

r = function () { new (x(y).n::z); };
expect = 'function () { new (x(y).n::z); }';
actual = r + '';
compareSource(expect, actual, inSection(++nTests) + summary);

// ---------------------------------------------------------------

l = function () { (new x(y)).n::[z]; };
expect = 'function () { (new x(y)).n::[z]; }';
actual = l + '';
compareSource(expect, actual, inSection(++nTests) + summary);

m = function () { new (x(y)).n::[z]; };
expect = 'function () { new (x(y).n::[z]); }';
actual = m + '';
compareSource(expect, actual, inSection(++nTests) + summary);

r = function () { new (x(y).n::[z]); };
expect = 'function () { new (x(y).n::[z]); }';
actual = r + '';
compareSource(expect, actual, inSection(++nTests) + summary);

END();
