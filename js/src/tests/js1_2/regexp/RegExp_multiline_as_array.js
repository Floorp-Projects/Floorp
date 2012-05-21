/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     RegExp_multiline_as_array.js
   Description:  'Tests RegExps $* property  (same tests as RegExp_multiline.js but using $*)'

   Author:       Nick Lerissa
   Date:         March 13, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: $*';

writeHeaderToLog('Executing script: RegExp_multiline_as_array.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// First we do a series of tests with RegExp['$*'] set to false (default value)
// Following this we do the same tests with RegExp['$*'] set true(**).
// RegExp['$*']
new TestCase ( SECTION, "RegExp['$*']",
	       false, RegExp['$*']);

// (['$*'] == false) '123\n456'.match(/^4../)
new TestCase ( SECTION, "(['$*'] == false) '123\\n456'.match(/^4../)",
	       null, '123\n456'.match(/^4../));

// (['$*'] == false) 'a11\na22\na23\na24'.match(/^a../g)
new TestCase ( SECTION, "(['$*'] == false) 'a11\\na22\\na23\\na24'.match(/^a../g)",
	       String(['a11']), String('a11\na22\na23\na24'.match(/^a../g)));

// (['$*'] == false) 'a11\na22'.match(/^.+^./)
new TestCase ( SECTION, "(['$*'] == false) 'a11\na22'.match(/^.+^./)",
	       null, 'a11\na22'.match(/^.+^./));

// (['$*'] == false) '123\n456'.match(/.3$/)
new TestCase ( SECTION, "(['$*'] == false) '123\\n456'.match(/.3$/)",
	       null, '123\n456'.match(/.3$/));

// (['$*'] == false) 'a11\na22\na23\na24'.match(/a..$/g)
new TestCase ( SECTION, "(['$*'] == false) 'a11\\na22\\na23\\na24'.match(/a..$/g)",
	       String(['a24']), String('a11\na22\na23\na24'.match(/a..$/g)));

// (['$*'] == false) 'abc\ndef'.match(/c$...$/)
new TestCase ( SECTION, "(['$*'] == false) 'abc\ndef'.match(/c$...$/)",
	       null, 'abc\ndef'.match(/c$...$/));

// (['$*'] == false) 'a11\na22\na23\na24'.match(new RegExp('a..$','g'))
new TestCase ( SECTION, "(['$*'] == false) 'a11\\na22\\na23\\na24'.match(new RegExp('a..$','g'))",
	       String(['a24']), String('a11\na22\na23\na24'.match(new RegExp('a..$','g'))));

// (['$*'] == false) 'abc\ndef'.match(new RegExp('c$...$'))
new TestCase ( SECTION, "(['$*'] == false) 'abc\ndef'.match(new RegExp('c$...$'))",
	       null, 'abc\ndef'.match(new RegExp('c$...$')));

// **Now we do the tests with RegExp['$*'] set to true
// RegExp['$*'] = true; RegExp['$*']
RegExp['$*'] = true;
new TestCase ( SECTION, "RegExp['$*'] = true; RegExp['$*']",
	       true, RegExp['$*']);

// (['$*'] == true) '123\n456'.match(/^4../)
new TestCase ( SECTION, "(['$*'] == true) '123\\n456'.match(/^4../)",
	       String(['456']), String('123\n456'.match(/^4../)));

// (['$*'] == true) 'a11\na22\na23\na24'.match(/^a../g)
new TestCase ( SECTION, "(['$*'] == true) 'a11\\na22\\na23\\na24'.match(/^a../g)",
	       String(['a11','a22','a23','a24']), String('a11\na22\na23\na24'.match(/^a../g)));

// (['$*'] == true) 'a11\na22'.match(/^.+^./)
//new TestCase ( SECTION, "(['$*'] == true) 'a11\na22'.match(/^.+^./)",
//                                    String(['a11\na']), String('a11\na22'.match(/^.+^./)));

// (['$*'] == true) '123\n456'.match(/.3$/)
new TestCase ( SECTION, "(['$*'] == true) '123\\n456'.match(/.3$/)",
	       String(['23']), String('123\n456'.match(/.3$/)));

// (['$*'] == true) 'a11\na22\na23\na24'.match(/a..$/g)
new TestCase ( SECTION, "(['$*'] == true) 'a11\\na22\\na23\\na24'.match(/a..$/g)",
	       String(['a11','a22','a23','a24']), String('a11\na22\na23\na24'.match(/a..$/g)));

// (['$*'] == true) 'a11\na22\na23\na24'.match(new RegExp('a..$','g'))
new TestCase ( SECTION, "(['$*'] == true) 'a11\\na22\\na23\\na24'.match(new RegExp('a..$','g'))",
	       String(['a11','a22','a23','a24']), String('a11\na22\na23\na24'.match(new RegExp('a..$','g'))));

// (['$*'] == true) 'abc\ndef'.match(/c$....$/)
//new TestCase ( SECTION, "(['$*'] == true) 'abc\ndef'.match(/c$.+$/)",
//                                    'c\ndef', String('abc\ndef'.match(/c$.+$/)));

RegExp['$*'] = false;

test();
