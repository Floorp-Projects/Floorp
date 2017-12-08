/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          label-002.js
 *  ECMA Section:
 *  Description:        Labeled statements
 *
 *  Labeled break and continue within a for-in loop.
 *
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "label-002";
var TITLE   = "Labeled statements";

writeHeaderToLog( SECTION + " "+ TITLE);

LabelTest( { p1:"hi,", p2:" norris" }, "hi, norris", " norrishi," );
LabelTest( { 0:"zero", 1:"one" }, "zeroone", "onezero" );

LabelTest2( { p1:"hi,", p2:" norris" }, "hi,", " norris" );
LabelTest2( { 0:"zero", 1:"one" }, "zero", "one" );

test();

function LabelTest( object, expect1, expect2 ) {
  result = "";

yoohoo:  { for ( property in object ) { result += object[property]; }; break yoohoo };

  new TestCase(
    "yoohoo: for ( property in object ) { result += object[property]; } break yoohoo }",
    true,
    result == expect1 || result == expect2 );
}

function LabelTest2( object, expect1, expect2 ) {
  result = "";

yoohoo:  { for ( property in object ) { result += object[property]; break yoohoo } }; ;

  new TestCase(
    "yoohoo: for ( property in object ) { result += object[property]; break yoohoo }}",
    true,
    result == expect1 || result == expect2 );
}

