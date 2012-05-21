// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          RegExp/exec-001.js
 *  ECMA Section:       15.7.5.3
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */
var SECTION = "RegExp/exec-001";
var VERSION = "ECMA_2";
var TITLE   = "RegExp.prototype.exec(string)";

startTest();

/*
 * for each test case, verify:
 * - type of object returned
 * - length of the returned array
 * - value of lastIndex
 * - value of index
 * - value of input
 * - value of the array indices
 */

// test cases without subpatterns
// test cases with subpatterns
// global property is true
// global property is false
// test cases in which the exec returns null

AddTestCase("NO TESTS EXIST", "PASSED", "Test not implemented");

test();

