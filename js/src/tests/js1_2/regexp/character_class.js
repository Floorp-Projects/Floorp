/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     character_class.js
   Description:  'Tests regular expressions containing []'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE = 'RegExp: []';

writeHeaderToLog('Executing script: character_class.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'abcde'.match(new RegExp('ab[ercst]de'))
new TestCase ( "'abcde'.match(new RegExp('ab[ercst]de'))",
	       String(["abcde"]), String('abcde'.match(new RegExp('ab[ercst]de'))));

// 'abcde'.match(new RegExp('ab[erst]de'))
new TestCase ( "'abcde'.match(new RegExp('ab[erst]de'))",
	       null, 'abcde'.match(new RegExp('ab[erst]de')));

// 'abcdefghijkl'.match(new RegExp('[d-h]+'))
new TestCase ( "'abcdefghijkl'.match(new RegExp('[d-h]+'))",
	       String(["defgh"]), String('abcdefghijkl'.match(new RegExp('[d-h]+'))));

// 'abc6defghijkl'.match(new RegExp('[1234567].{2}'))
new TestCase ( "'abc6defghijkl'.match(new RegExp('[1234567].{2}'))",
	       String(["6de"]), String('abc6defghijkl'.match(new RegExp('[1234567].{2}'))));

// '\n\n\abc324234\n'.match(new RegExp('[a-c\d]+'))
new TestCase ( "'\n\n\abc324234\n'.match(new RegExp('[a-c\\d]+'))",
	       String(["abc324234"]), String('\n\n\abc324234\n'.match(new RegExp('[a-c\\d]+'))));

// 'abc'.match(new RegExp('ab[.]?c'))
new TestCase ( "'abc'.match(new RegExp('ab[.]?c'))",
	       String(["abc"]), String('abc'.match(new RegExp('ab[.]?c'))));

// 'abc'.match(new RegExp('a[b]c'))
new TestCase ( "'abc'.match(new RegExp('a[b]c'))",
	       String(["abc"]), String('abc'.match(new RegExp('a[b]c'))));

// 'a1b  b2c  c3d  def  f4g'.match(new RegExp('[a-z][^1-9][a-z]'))
new TestCase ( "'a1b  b2c  c3d  def  f4g'.match(new RegExp('[a-z][^1-9][a-z]'))",
	       String(["def"]), String('a1b  b2c  c3d  def  f4g'.match(new RegExp('[a-z][^1-9][a-z]'))));

// '123*&$abc'.match(new RegExp('[*&$]{3}'))
new TestCase ( "'123*&$abc'.match(new RegExp('[*&$]{3}'))",
	       String(["*&$"]), String('123*&$abc'.match(new RegExp('[*&$]{3}'))));

// 'abc'.match(new RegExp('a[^1-9]c'))
new TestCase ( "'abc'.match(new RegExp('a[^1-9]c'))",
	       String(["abc"]), String('abc'.match(new RegExp('a[^1-9]c'))));

// 'abc'.match(new RegExp('a[^b]c'))
new TestCase ( "'abc'.match(new RegExp('a[^b]c'))",
	       null, 'abc'.match(new RegExp('a[^b]c')));

// 'abc#$%def%&*@ghi)(*&'.match(new RegExp('[^a-z]{4}'))
new TestCase ( "'abc#$%def%&*@ghi)(*&'.match(new RegExp('[^a-z]{4}'))",
	       String(["%&*@"]), String('abc#$%def%&*@ghi)(*&'.match(new RegExp('[^a-z]{4}'))));

// 'abc#$%def%&*@ghi)(*&'.match(/[^a-z]{4}/)
new TestCase ( "'abc#$%def%&*@ghi)(*&'.match(/[^a-z]{4}/)",
	       String(["%&*@"]), String('abc#$%def%&*@ghi)(*&'.match(/[^a-z]{4}/)));

test();
