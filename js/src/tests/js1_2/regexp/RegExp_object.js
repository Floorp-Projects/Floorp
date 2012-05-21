/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     RegExp_object.js
   Description:  'Tests regular expressions creating RexExp Objects'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: object';

writeHeaderToLog('Executing script: RegExp_object.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var SSN_pattern = new RegExp("\\d{3}-\\d{2}-\\d{4}");

// testing SSN pattern
new TestCase ( SECTION, "'Test SSN is 123-34-4567'.match(SSN_pattern))",
	       String(["123-34-4567"]), String('Test SSN is 123-34-4567'.match(SSN_pattern)));

// testing SSN pattern
new TestCase ( SECTION, "'Test SSN is 123-34-4567'.match(SSN_pattern))",
	       String(["123-34-4567"]), String('Test SSN is 123-34-4567'.match(SSN_pattern)));

var PHONE_pattern = new RegExp("\\(?(\\d{3})\\)?-?(\\d{3})-(\\d{4})");
// testing PHONE pattern
new TestCase ( SECTION, "'Our phone number is (408)345-2345.'.match(PHONE_pattern))",
	       String(["(408)345-2345","408","345","2345"]), String('Our phone number is (408)345-2345.'.match(PHONE_pattern)));

// testing PHONE pattern
new TestCase ( SECTION, "'The phone number is 408-345-2345!'.match(PHONE_pattern))",
	       String(["408-345-2345","408","345","2345"]), String('The phone number is 408-345-2345!'.match(PHONE_pattern)));

// testing PHONE pattern
new TestCase ( SECTION, "String(PHONE_pattern.toString())",
	       "/\\(?(\\d{3})\\)?-?(\\d{3})-(\\d{4})/", String(PHONE_pattern.toString()));

// testing conversion to String
new TestCase ( SECTION, "PHONE_pattern + ' is the string'",
	       "/\\(?(\\d{3})\\)?-?(\\d{3})-(\\d{4})/ is the string",PHONE_pattern + ' is the string');

// testing conversion to int
new TestCase ( SECTION, "SSN_pattern - 8",
	       NaN,SSN_pattern - 8);

var testPattern = new RegExp("(\\d+)45(\\d+)90");

test();
