/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.7.3-1.js
   ECMA Section:       7.7.3 Numeric Literals

   Description:        A numeric literal stands for a value of the Number type
   This value is determined in two steps:  first a
   mathematical value (MV) is derived from the literal;
   second, this mathematical value is rounded, ideally
   using IEEE 754 round-to-nearest mode, to a reprentable
   value of of the number type.

   These test cases came from Waldemar.

   Author:             christine@netscape.com
   Date:               12 June 1998
*/

var SECTION = "7.7.3-1";
var TITLE   = "Numeric Literals";
var BUGNUMBER="122877";

printBugNumber(BUGNUMBER);

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(
	      "0x12345678",
	      305419896,
	      0x12345678 );

new TestCase(
	      "0x80000000",
	      2147483648,
	      0x80000000 );

new TestCase(
	      "0xffffffff",
	      4294967295,
	      0xffffffff );

new TestCase(
	      "0x100000000",
	      4294967296,
	      0x100000000 );

new TestCase(
	      "077777777777777777",
	      2251799813685247,
	      077777777777777777 );

new TestCase(
	      "077777777777777776",
	      2251799813685246,
	      077777777777777776 );

new TestCase(
	      "0x1fffffffffffff",
	      9007199254740991,
	      0x1fffffffffffff );

new TestCase(
	      "0x20000000000000",
	      9007199254740992,
	      0x20000000000000 );

new TestCase(
	      "0x20123456789abc",
	      9027215253084860,
	      0x20123456789abc );

new TestCase(
	      "0x20123456789abd",
	      9027215253084860,
	      0x20123456789abd );

new TestCase(
	      "0x20123456789abe",
	      9027215253084862,
	      0x20123456789abe );

new TestCase(
	      "0x20123456789abf",
	      9027215253084864,
	      0x20123456789abf );

new TestCase(
	      "0x1000000000000080",
	      1152921504606847000,
	      0x1000000000000080 );

new TestCase(
	      "0x1000000000000081",
	      1152921504606847200,
	      0x1000000000000081 );

new TestCase(
	      "0x1000000000000100",
	      1152921504606847200,
	      0x1000000000000100 );

new TestCase(
	      "0x100000000000017f",
	      1152921504606847200,
	      0x100000000000017f );

new TestCase(
	      "0x1000000000000180",
	      1152921504606847500,
	      0x1000000000000180 );

new TestCase(
	      "0x1000000000000181",
	      1152921504606847500,
	      0x1000000000000181 );

new TestCase(
	      "0x10000000000001f0",
	      1152921504606847500,
	      0x10000000000001f0 );

new TestCase(
	      "0x1000000000000200",
	      1152921504606847500,
	      0x1000000000000200 );

new TestCase(
	      "0x100000000000027f",
	      1152921504606847500,
	      0x100000000000027f );

new TestCase(
	      "0x1000000000000280",
	      1152921504606847500,
	      0x1000000000000280 );

new TestCase(
	      "0x1000000000000281",
	      1152921504606847700,
	      0x1000000000000281 );

new TestCase(
	      "0x10000000000002ff",
	      1152921504606847700,
	      0x10000000000002ff );

new TestCase(
	      "0x1000000000000300",
	      1152921504606847700,
	      0x1000000000000300 );

new TestCase(
	      "0x10000000000000000",
	      18446744073709552000,
	      0x10000000000000000 );

test();

