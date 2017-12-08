/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 415540;
var summary = 'Array.push'
var actual = '';
var expect = '';

var Constr = function() {};
Constr.prototype = [];
var c = new Constr();
c.push(5);
c.push(6);
var actual = Array.push(c,7);
reportCompare(3, actual, "result of Array.push is length");
reportCompare(5, c[0], "contents of c[0]");
reportCompare(6, c[1], "contents of c[1]");
reportCompare(7, c[2], "contents of c[2]");
