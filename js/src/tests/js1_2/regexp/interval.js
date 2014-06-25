/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     interval.js
   Description:  'Tests regular expressions containing {}'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: {}';

writeHeaderToLog('Executing script: interval.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{2}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{2}c'))",
	       String(["bbc"]), String('aaabbbbcccddeeeefffff'.match(new RegExp('b{2}c'))));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{8}'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{8}'))",
	       null, 'aaabbbbcccddeeeefffff'.match(new RegExp('b{8}')));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{2,}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{2,}c'))",
	       String(["bbbbc"]), String('aaabbbbcccddeeeefffff'.match(new RegExp('b{2,}c'))));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{8,}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{8,}c'))",
	       null, 'aaabbbbcccddeeeefffff'.match(new RegExp('b{8,}c')));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{2,3}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{2,3}c'))",
	       String(["bbbc"]), String('aaabbbbcccddeeeefffff'.match(new RegExp('b{2,3}c'))));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{42,93}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{42,93}c'))",
	       null, 'aaabbbbcccddeeeefffff'.match(new RegExp('b{42,93}c')));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('b{0,93}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('b{0,93}c'))",
	       String(["bbbbc"]), String('aaabbbbcccddeeeefffff'.match(new RegExp('b{0,93}c'))));

// 'aaabbbbcccddeeeefffff'.match(new RegExp('bx{0,93}c'))
new TestCase ( SECTION, "'aaabbbbcccddeeeefffff'.match(new RegExp('bx{0,93}c'))",
	       String(["bc"]), String('aaabbbbcccddeeeefffff'.match(new RegExp('bx{0,93}c'))));

// 'weirwerdf'.match(new RegExp('.{0,93}'))
new TestCase ( SECTION, "'weirwerdf'.match(new RegExp('.{0,93}'))",
	       String(["weirwerdf"]), String('weirwerdf'.match(new RegExp('.{0,93}'))));

// 'wqe456646dsff'.match(new RegExp('\d{1,}'))
new TestCase ( SECTION, "'wqe456646dsff'.match(new RegExp('\\d{1,}'))",
	       String(["456646"]), String('wqe456646dsff'.match(new RegExp('\\d{1,}'))));

// '123123'.match(new RegExp('(123){1,}'))
new TestCase ( SECTION, "'123123'.match(new RegExp('(123){1,}'))",
	       String(["123123","123"]), String('123123'.match(new RegExp('(123){1,}'))));

// '123123x123'.match(new RegExp('(123){1,}x\1'))
new TestCase ( SECTION, "'123123x123'.match(new RegExp('(123){1,}x\\1'))",
	       String(["123123x123","123"]), String('123123x123'.match(new RegExp('(123){1,}x\\1'))));

// '123123x123'.match(/(123){1,}x\1/)
new TestCase ( SECTION, "'123123x123'.match(/(123){1,}x\\1/)",
	       String(["123123x123","123"]), String('123123x123'.match(/(123){1,}x\1/)));

// 'xxxxxxx'.match(new RegExp('x{1,2}x{1,}'))
new TestCase ( SECTION, "'xxxxxxx'.match(new RegExp('x{1,2}x{1,}'))",
	       String(["xxxxxxx"]), String('xxxxxxx'.match(new RegExp('x{1,2}x{1,}'))));

// 'xxxxxxx'.match(/x{1,2}x{1,}/)
new TestCase ( SECTION, "'xxxxxxx'.match(/x{1,2}x{1,}/)",
	       String(["xxxxxxx"]), String('xxxxxxx'.match(/x{1,2}x{1,}/)));

test();
