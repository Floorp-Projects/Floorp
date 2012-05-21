/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          RegExp/function-001.js
 *  ECMA Section:       15.7.2.1
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */
var SECTION = "RegExp/function-001";
var VERSION = "ECMA_2";
var TITLE   = "RegExp( pattern, flags )";

startTest();

/*
 * for each test case, verify:
 * - verify that [[Class]] property is RegExp
 * - prototype property should be set to RegExp.prototype
 * - source is set to the empty string
 * - global property is set to false
 * - ignoreCase property is set to false
 * - multiline property is set to false
 * - lastIndex property is set to 0
 */

RegExp.prototype.getClassProperty = Object.prototype.toString;
var re = new RegExp();

AddTestCase(
  "RegExp.prototype.getClassProperty = Object.prototype.toString; " +
  "(new RegExp()).getClassProperty()",
  "[object RegExp]",
  re.getClassProperty() );

AddTestCase(
  "(new RegExp()).source",
  "",
  re.source );

AddTestCase(
  "(new RegExp()).global",
  false,
  re.global );

AddTestCase(
  "(new RegExp()).ignoreCase",
  false,
  re.ignoreCase );

AddTestCase(
  "(new RegExp()).multiline",
  false,
  re.multiline );

AddTestCase(
  "(new RegExp()).lastIndex",
  0,
  re.lastIndex );

test()
