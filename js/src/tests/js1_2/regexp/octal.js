/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     octal.js
   Description:  'Tests regular expressions containing \<number> '

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: \# (octal) ';

writeHeaderToLog('Executing script: octal.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var testPattern = '\\101\\102\\103\\104\\105\\106\\107\\110\\111\\112\\113\\114\\115\\116\\117\\120\\121\\122\\123\\124\\125\\126\\127\\130\\131\\132';

var testString = "12345ABCDEFGHIJKLMNOPQRSTUVWXYZ67890";

new TestCase ( SECTION,
	       "'" + testString + "'.match(new RegExp('" + testPattern + "'))",
	       String(["ABCDEFGHIJKLMNOPQRSTUVWXYZ"]), String(testString.match(new RegExp(testPattern))));

testPattern = '\\141\\142\\143\\144\\145\\146\\147\\150\\151\\152\\153\\154\\155\\156\\157\\160\\161\\162\\163\\164\\165\\166\\167\\170\\171\\172';

testString = "12345AabcdefghijklmnopqrstuvwxyzZ67890";

new TestCase ( SECTION,
	       "'" + testString + "'.match(new RegExp('" + testPattern + "'))",
	       String(["abcdefghijklmnopqrstuvwxyz"]), String(testString.match(new RegExp(testPattern))));

testPattern = '\\40\\41\\42\\43\\44\\45\\46\\47\\50\\51\\52\\53\\54\\55\\56\\57\\60\\61\\62\\63';

testString = "abc !\"#$%&'()*+,-./0123ZBC";

new TestCase ( SECTION,
	       "'" + testString + "'.match(new RegExp('" + testPattern + "'))",
	       String([" !\"#$%&'()*+,-./0123"]), String(testString.match(new RegExp(testPattern))));

testPattern = '\\64\\65\\66\\67\\70\\71\\72\\73\\74\\75\\76\\77\\100';

testString = "123456789:;<=>?@ABC";

new TestCase ( SECTION,
	       "'" + testString + "'.match(new RegExp('" + testPattern + "'))",
	       String(["456789:;<=>?@"]), String(testString.match(new RegExp(testPattern))));

testPattern = '\\173\\174\\175\\176';

testString = "1234{|}~ABC";

new TestCase ( SECTION,
	       "'" + testString + "'.match(new RegExp('" + testPattern + "'))",
	       String(["{|}~"]), String(testString.match(new RegExp(testPattern))));

new TestCase ( SECTION,
	       "'canthisbeFOUND'.match(new RegExp('[A-\\132]+'))",
	       String(["FOUND"]), String('canthisbeFOUND'.match(new RegExp('[A-\\132]+'))));

new TestCase ( SECTION,
	       "'canthisbeFOUND'.match(new RegExp('[\\141-\\172]+'))",
	       String(["canthisbe"]), String('canthisbeFOUND'.match(new RegExp('[\\141-\\172]+'))));

new TestCase ( SECTION,
	       "'canthisbeFOUND'.match(/[\\141-\\172]+/)",
	       String(["canthisbe"]), String('canthisbeFOUND'.match(/[\141-\172]+/)));

test();
