/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          6-1.js
   ECMA Section:       Source Text
   Description:

   ECMAScript source text is represented as a sequence of characters
   representable using the Unicode version 2.0 character encoding.

   SourceCharacter ::
   any Unicode character

   However, it is possible to represent every ECMAScript program using
   only ASCII characters (which are equivalent to the first 128 Unicode
   characters). Non-ASCII Unicode characters may appear only within comments
   and string literals. In string literals, any Unicode character may also be
   expressed as a Unicode escape sequence consisting of six ASCII characters,
   namely \u plus four hexadecimal digits. Within a comment, such an escape
   sequence is effectively ignored as part of the comment. Within a string
   literal, the Unicode escape sequence contributes one character to the string
   value of the literal.

   Note that ECMAScript differs from the Java programming language in the
   behavior of Unicode escape sequences. In a Java program, if the Unicode escape
   sequence \u000A, for example, occurs within a single-line comment, it is
   interpreted as a line terminator (Unicode character 000A is line feed) and
   therefore the next character is not part of the comment. Similarly, if the
   Unicode escape sequence \u000A occurs within a string literal in a Java
   program, it is likewise interpreted as a line terminator, which is not
   allowed within a string literal-one must write \n instead of \u000A to
   cause a line feed to be part of the string value of a string literal. In
   an ECMAScript program, a Unicode escape sequence occurring within a comment
   is never interpreted and therefore cannot contribute to termination of the
   comment. Similarly, a Unicode escape sequence occurring within a string literal
   in an ECMAScript program always contributes a character to the string value of
   the literal and is never interpreted as a line terminator or as a quote mark
   that might terminate the string literal.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "6-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Source Text";

writeHeaderToLog( SECTION + " "+ TITLE);

var testcase = new TestCase( SECTION,
			     "// the following character should not be interpreted as a line terminator in a comment: \u000A",
			     'PASSED',
			     "PASSED" );

// \u000A testcase.actual = "FAILED!";

testcase =
  new TestCase( SECTION,
		"// the following character should not be interpreted as a line terminator in a comment: \\n 'FAILED'",
		'PASSED',
		'PASSED' );

// the following character should noy be interpreted as a line terminator: \\n testcase.actual = "FAILED"

testcase =
  new TestCase( SECTION,
		"// the following character should not be interpreted as a line terminator in a comment: \\u000A 'FAILED'",
		'PASSED',
		'PASSED' );

// the following character should not be interpreted as a line terminator:   \u000A testcase.actual = "FAILED"

testcase =
  new TestCase( SECTION,
		"// the following character should not be interpreted as a line terminator in a comment: \n 'PASSED'",
		'PASSED',
		'PASSED' );
// the following character should not be interpreted as a line terminator: \n testcase.actual = 'FAILED'

testcase =
  new TestCase(   SECTION,
		  "// the following character should not be interpreted as a line terminator in a comment: u000D",
		  'PASSED',
		  'PASSED' );

// the following character should not be interpreted as a line terminator:   \u000D testcase.actual = "FAILED"

test();

