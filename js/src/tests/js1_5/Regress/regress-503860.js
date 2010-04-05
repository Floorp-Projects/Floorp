/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

var gTestfile = 'regress-503860.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 503860;
var summary = "Don't shadow a readonly or setter proto-property";
var expect = "PASS";
var actual = "FAIL";
var a = {y: 1};

function B(){}
B.prototype.__defineSetter__('x', function setx(val) { actual = expect; });
var b = new B;
b.y = 1;

var arr = [a, b];       // same shape prior to bug 497789 fix
for each (var obj in arr)
    obj.x = 2;          // should call b's setter but doesn't

reportCompare(expect, actual, summary);
