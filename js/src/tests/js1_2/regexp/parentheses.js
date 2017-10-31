/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     parentheses.js
   Description:  'Tests regular expressions containing ()'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE   = 'RegExp: ()';

writeHeaderToLog('Executing script: parentheses.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abc'.match(new RegExp('(abc)'))
new TestCase ( "'abc'.match(new RegExp('(abc)'))",
	       String(["abc","abc"]), String('abc'.match(new RegExp('(abc)'))));

// 'abcdefg'.match(new RegExp('a(bc)d(ef)g'))
new TestCase ( "'abcdefg'.match(new RegExp('a(bc)d(ef)g'))",
	       String(["abcdefg","bc","ef"]), String('abcdefg'.match(new RegExp('a(bc)d(ef)g'))));

// 'abcdefg'.match(new RegExp('(.{3})(.{4})'))
new TestCase ( "'abcdefg'.match(new RegExp('(.{3})(.{4})'))",
	       String(["abcdefg","abc","defg"]), String('abcdefg'.match(new RegExp('(.{3})(.{4})'))));

// 'aabcdaabcd'.match(new RegExp('(aa)bcd\1'))
new TestCase ( "'aabcdaabcd'.match(new RegExp('(aa)bcd\\1'))",
	       String(["aabcdaa","aa"]), String('aabcdaabcd'.match(new RegExp('(aa)bcd\\1'))));

// 'aabcdaabcd'.match(new RegExp('(aa).+\1'))
new TestCase ( "'aabcdaabcd'.match(new RegExp('(aa).+\\1'))",
	       String(["aabcdaa","aa"]), String('aabcdaabcd'.match(new RegExp('(aa).+\\1'))));

// 'aabcdaabcd'.match(new RegExp('(.{2}).+\1'))
new TestCase ( "'aabcdaabcd'.match(new RegExp('(.{2}).+\\1'))",
	       String(["aabcdaa","aa"]), String('aabcdaabcd'.match(new RegExp('(.{2}).+\\1'))));

// '123456123456'.match(new RegExp('(\d{3})(\d{3})\1\2'))
new TestCase ( "'123456123456'.match(new RegExp('(\\d{3})(\\d{3})\\1\\2'))",
	       String(["123456123456","123","456"]), String('123456123456'.match(new RegExp('(\\d{3})(\\d{3})\\1\\2'))));

// 'abcdefg'.match(new RegExp('a(..(..)..)'))
new TestCase ( "'abcdefg'.match(new RegExp('a(..(..)..)'))",
	       String(["abcdefg","bcdefg","de"]), String('abcdefg'.match(new RegExp('a(..(..)..)'))));

// 'abcdefg'.match(/a(..(..)..)/)
new TestCase ( "'abcdefg'.match(/a(..(..)..)/)",
	       String(["abcdefg","bcdefg","de"]), String('abcdefg'.match(/a(..(..)..)/)));

// 'xabcdefg'.match(new RegExp('(a(b(c)))(d(e(f)))'))
new TestCase ( "'xabcdefg'.match(new RegExp('(a(b(c)))(d(e(f)))'))",
	       String(["abcdef","abc","bc","c","def","ef","f"]), String('xabcdefg'.match(new RegExp('(a(b(c)))(d(e(f)))'))));

// 'xabcdefbcefg'.match(new RegExp('(a(b(c)))(d(e(f)))\2\5'))
new TestCase ( "'xabcdefbcefg'.match(new RegExp('(a(b(c)))(d(e(f)))\\2\\5'))",
	       String(["abcdefbcef","abc","bc","c","def","ef","f"]), String('xabcdefbcefg'.match(new RegExp('(a(b(c)))(d(e(f)))\\2\\5'))));

// 'abcd'.match(new RegExp('a(.?)b\1c\1d\1'))
new TestCase ( "'abcd'.match(new RegExp('a(.?)b\\1c\\1d\\1'))",
	       String(["abcd",""]), String('abcd'.match(new RegExp('a(.?)b\\1c\\1d\\1'))));

// 'abcd'.match(/a(.?)b\1c\1d\1/)
new TestCase ( "'abcd'.match(/a(.?)b\\1c\\1d\\1/)",
	       String(["abcd",""]), String('abcd'.match(/a(.?)b\1c\1d\1/)));

test();
