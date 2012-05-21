/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     asterisk.js
   Description:  'Tests regular expressions containing *'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: *';

writeHeaderToLog('Executing script: aterisk.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abcddddefg'.match(new RegExp('d*'))
new TestCase ( SECTION, "'abcddddefg'.match(new RegExp('d*'))",
	       String([""]), String('abcddddefg'.match(new RegExp('d*'))));

// 'abcddddefg'.match(new RegExp('cd*'))
new TestCase ( SECTION, "'abcddddefg'.match(new RegExp('cd*'))",
	       String(["cdddd"]), String('abcddddefg'.match(new RegExp('cd*'))));

// 'abcdefg'.match(new RegExp('cx*d'))
new TestCase ( SECTION, "'abcdefg'.match(new RegExp('cx*d'))",
	       String(["cd"]), String('abcdefg'.match(new RegExp('cx*d'))));

// 'xxxxxxx'.match(new RegExp('(x*)(x+)'))
new TestCase ( SECTION, "'xxxxxxx'.match(new RegExp('(x*)(x+)'))",
	       String(["xxxxxxx","xxxxxx","x"]), String('xxxxxxx'.match(new RegExp('(x*)(x+)'))));

// '1234567890'.match(new RegExp('(\\d*)(\\d+)'))
new TestCase ( SECTION, "'1234567890'.match(new RegExp('(\\d*)(\\d+)'))",
	       String(["1234567890","123456789","0"]),
	       String('1234567890'.match(new RegExp('(\\d*)(\\d+)'))));

// '1234567890'.match(new RegExp('(\\d*)\\d(\\d+)'))
new TestCase ( SECTION, "'1234567890'.match(new RegExp('(\\d*)\\d(\\d+)'))",
	       String(["1234567890","12345678","0"]),
	       String('1234567890'.match(new RegExp('(\\d*)\\d(\\d+)'))));

// 'xxxxxxx'.match(new RegExp('(x+)(x*)'))
new TestCase ( SECTION, "'xxxxxxx'.match(new RegExp('(x+)(x*)'))",
	       String(["xxxxxxx","xxxxxxx",""]), String('xxxxxxx'.match(new RegExp('(x+)(x*)'))));

// 'xxxxxxyyyyyy'.match(new RegExp('x*y+$'))
new TestCase ( SECTION, "'xxxxxxyyyyyy'.match(new RegExp('x*y+$'))",
	       String(["xxxxxxyyyyyy"]), String('xxxxxxyyyyyy'.match(new RegExp('x*y+$'))));

// 'abcdef'.match(/[\d]*[\s]*bc./)
new TestCase ( SECTION, "'abcdef'.match(/[\\d]*[\\s]*bc./)",
	       String(["bcd"]), String('abcdef'.match(/[\d]*[\s]*bc./)));

// 'abcdef'.match(/bc..[\d]*[\s]*/)
new TestCase ( SECTION, "'abcdef'.match(/bc..[\\d]*[\\s]*/)",
	       String(["bcde"]), String('abcdef'.match(/bc..[\d]*[\s]*/)));

// 'a1b2c3'.match(/.*/)
new TestCase ( SECTION, "'a1b2c3'.match(/.*/)",
	       String(["a1b2c3"]), String('a1b2c3'.match(/.*/)));

// 'a0.b2.c3'.match(/[xyz]*1/)
new TestCase ( SECTION, "'a0.b2.c3'.match(/[xyz]*1/)",
	       null, 'a0.b2.c3'.match(/[xyz]*1/));

test();
