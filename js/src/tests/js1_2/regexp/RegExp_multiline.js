/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     RegExp_multiline.js
   Description:  'Tests RegExps multiline property'

   Author:       Nick Lerissa
   Date:         March 12, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: multiline';

writeHeaderToLog('Executing script: RegExp_multiline.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// First we do a series of tests with RegExp.multiline set to false (default value)
// Following this we do the same tests with RegExp.multiline set true(**).
// RegExp.multiline
new TestCase ( SECTION, "RegExp.multiline",
	       false, RegExp.multiline);

// (multiline == false) '123\n456'.match(/^4../)
new TestCase ( SECTION, "(multiline == false) '123\\n456'.match(/^4../)",
	       null, '123\n456'.match(/^4../));

// (multiline == false) 'a11\na22\na23\na24'.match(/^a../g)
new TestCase ( SECTION, "(multiline == false) 'a11\\na22\\na23\\na24'.match(/^a../g)",
	       String(['a11']), String('a11\na22\na23\na24'.match(/^a../g)));

// (multiline == false) 'a11\na22'.match(/^.+^./)
new TestCase ( SECTION, "(multiline == false) 'a11\na22'.match(/^.+^./)",
	       null, 'a11\na22'.match(/^.+^./));

// (multiline == false) '123\n456'.match(/.3$/)
new TestCase ( SECTION, "(multiline == false) '123\\n456'.match(/.3$/)",
	       null, '123\n456'.match(/.3$/));

// (multiline == false) 'a11\na22\na23\na24'.match(/a..$/g)
new TestCase ( SECTION, "(multiline == false) 'a11\\na22\\na23\\na24'.match(/a..$/g)",
	       String(['a24']), String('a11\na22\na23\na24'.match(/a..$/g)));

// (multiline == false) 'abc\ndef'.match(/c$...$/)
new TestCase ( SECTION, "(multiline == false) 'abc\ndef'.match(/c$...$/)",
	       null, 'abc\ndef'.match(/c$...$/));

// (multiline == false) 'a11\na22\na23\na24'.match(new RegExp('a..$','g'))
new TestCase ( SECTION, "(multiline == false) 'a11\\na22\\na23\\na24'.match(new RegExp('a..$','g'))",
	       String(['a24']), String('a11\na22\na23\na24'.match(new RegExp('a..$','g'))));

// (multiline == false) 'abc\ndef'.match(new RegExp('c$...$'))
new TestCase ( SECTION, "(multiline == false) 'abc\ndef'.match(new RegExp('c$...$'))",
	       null, 'abc\ndef'.match(new RegExp('c$...$')));

// **Now we do the tests with RegExp.multiline set to true
// RegExp.multiline = true; RegExp.multiline
RegExp.multiline = true;
new TestCase ( SECTION, "RegExp.multiline = true; RegExp.multiline",
	       true, RegExp.multiline);

// (multiline == true) '123\n456'.match(/^4../)
new TestCase ( SECTION, "(multiline == true) '123\\n456'.match(/^4../)",
	       String(['456']), String('123\n456'.match(/^4../)));

// (multiline == true) 'a11\na22\na23\na24'.match(/^a../g)
new TestCase ( SECTION, "(multiline == true) 'a11\\na22\\na23\\na24'.match(/^a../g)",
	       String(['a11','a22','a23','a24']), String('a11\na22\na23\na24'.match(/^a../g)));

// (multiline == true) 'a11\na22'.match(/^.+^./)
//new TestCase ( SECTION, "(multiline == true) 'a11\na22'.match(/^.+^./)",
//                                    String(['a11\na']), String('a11\na22'.match(/^.+^./)));

// (multiline == true) '123\n456'.match(/.3$/)
new TestCase ( SECTION, "(multiline == true) '123\\n456'.match(/.3$/)",
	       String(['23']), String('123\n456'.match(/.3$/)));

// (multiline == true) 'a11\na22\na23\na24'.match(/a..$/g)
new TestCase ( SECTION, "(multiline == true) 'a11\\na22\\na23\\na24'.match(/a..$/g)",
	       String(['a11','a22','a23','a24']), String('a11\na22\na23\na24'.match(/a..$/g)));

// (multiline == true) 'a11\na22\na23\na24'.match(new RegExp('a..$','g'))
new TestCase ( SECTION, "(multiline == true) 'a11\\na22\\na23\\na24'.match(new RegExp('a..$','g'))",
	       String(['a11','a22','a23','a24']), String('a11\na22\na23\na24'.match(new RegExp('a..$','g'))));

// (multiline == true) 'abc\ndef'.match(/c$....$/)
//new TestCase ( SECTION, "(multiline == true) 'abc\ndef'.match(/c$.+$/)",
//                                    'c\ndef', String('abc\ndef'.match(/c$.+$/)));

RegExp.multiline = false;

test();
