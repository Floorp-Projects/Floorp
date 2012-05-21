/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     test.js
   Description:  'Tests regular expressions method compile'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: test';

writeHeaderToLog('Executing script: test.js');
writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase ( SECTION,
	       "/[0-9]{3}/.test('23 2 34 678 9 09')",
	       true, /[0-9]{3}/.test('23 2 34 678 9 09'));

new TestCase ( SECTION,
	       "/[0-9]{3}/.test('23 2 34 78 9 09')",
	       false, /[0-9]{3}/.test('23 2 34 78 9 09'));

new TestCase ( SECTION,
	       "/\w+ \w+ \w+/.test('do a test')",
	       true, /\w+ \w+ \w+/.test("do a test"));

new TestCase ( SECTION,
	       "/\w+ \w+ \w+/.test('a test')",
	       false, /\w+ \w+ \w+/.test("a test"));

new TestCase ( SECTION,
	       "(new RegExp('[0-9]{3}')).test('23 2 34 678 9 09')",
	       true, (new RegExp('[0-9]{3}')).test('23 2 34 678 9 09'));

new TestCase ( SECTION,
	       "(new RegExp('[0-9]{3}')).test('23 2 34 78 9 09')",
	       false, (new RegExp('[0-9]{3}')).test('23 2 34 78 9 09'));

new TestCase ( SECTION,
	       "(new RegExp('\\\\w+ \\\\w+ \\\\w+')).test('do a test')",
	       true, (new RegExp('\\w+ \\w+ \\w+')).test("do a test"));

new TestCase ( SECTION,
	       "(new RegExp('\\\\w+ \\\\w+ \\\\w+')).test('a test')",
	       false, (new RegExp('\\w+ \\w+ \\w+')).test("a test"));

test();
