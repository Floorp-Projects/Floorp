/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          11.4.7-02.js
 *  Reference:          https://bugzilla.mozilla.org/show_bug.cgi?id=432881
 *  Description:        ecma 11.4.7
 */

var SECTION = "11.4.7";
var VERSION = "ECMA";
var TITLE   = "Unary - Operator";
var BUGNUMBER = "432881";

startTest();

test_negation(0, -0.0);
test_negation(-0.0, 0);
test_negation(1, -1);
test_negation(1.0/0.0, -1.0/0.0);
test_negation(-1.0/0.0, 1.0/0.0);

//1073741824 == (1 << 30)
test_negation(1073741824, -1073741824);
test_negation(-1073741824, 1073741824);

//1073741824 == (1 << 30) - 1
test_negation(1073741823, -1073741823);
test_negation(-1073741823, 1073741823);

//1073741824 == (1 << 30)
test_negation(1073741824, -1073741824);
test_negation(-1073741824, 1073741824);

//1073741824 == (1 << 30) - 1
test_negation(1073741823, -1073741823);
test_negation(-1073741823, 1073741823);

//2147483648 == (1 << 31)
test_negation(2147483648, -2147483648);
test_negation(-2147483648, 2147483648);

//2147483648 == (1 << 31) - 1
test_negation(2147483647, -2147483647);
test_negation(-2147483647, 2147483647);

function test_negation(value, expected)
{
    var actual = -value;
    reportCompare(expected, actual, '-(' + value + ') == ' + expected);
} 
