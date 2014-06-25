/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     RegExp_lastParen.js
   Description:  'Tests RegExps lastParen property'

   Author:       Nick Lerissa
   Date:         March 12, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: lastParen';

writeHeaderToLog('Executing script: RegExp_lastParen.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abcd'.match(/(abc)d/); RegExp.lastParen
'abcd'.match(/(abc)d/);
new TestCase ( SECTION, "'abcd'.match(/(abc)d/); RegExp.lastParen",
	       'abc', RegExp.lastParen);

// 'abcd'.match(new RegExp('(abc)d')); RegExp.lastParen
'abcd'.match(new RegExp('(abc)d'));
new TestCase ( SECTION, "'abcd'.match(new RegExp('(abc)d')); RegExp.lastParen",
	       'abc', RegExp.lastParen);

// 'abcd'.match(/(bcd)e/); RegExp.lastParen
'abcd'.match(/(bcd)e/);
new TestCase ( SECTION, "'abcd'.match(/(bcd)e/); RegExp.lastParen",
	       'abc', RegExp.lastParen);

// 'abcdefg'.match(/(a(b(c(d)e)f)g)/); RegExp.lastParen
'abcdefg'.match(/(a(b(c(d)e)f)g)/);
new TestCase ( SECTION, "'abcdefg'.match(/(a(b(c(d)e)f)g)/); RegExp.lastParen",
	       'd', RegExp.lastParen);

// 'abcdefg'.match(/(a(b)c)(d(e)f)/); RegExp.lastParen
'abcdefg'.match(/(a(b)c)(d(e)f)/);
new TestCase ( SECTION, "'abcdefg'.match(/(a(b)c)(d(e)f)/); RegExp.lastParen",
	       'e', RegExp.lastParen);

// 'abcdefg'.match(/(^)abc/); RegExp.lastParen
'abcdefg'.match(/(^)abc/);
new TestCase ( SECTION, "'abcdefg'.match(/(^)abc/); RegExp.lastParen",
	       '', RegExp.lastParen);

// 'abcdefg'.match(/(^a)bc/); RegExp.lastParen
'abcdefg'.match(/(^a)bc/);
new TestCase ( SECTION, "'abcdefg'.match(/(^a)bc/); RegExp.lastParen",
	       'a', RegExp.lastParen);

// 'abcdefg'.match(new RegExp('(^a)bc')); RegExp.lastParen
'abcdefg'.match(new RegExp('(^a)bc'));
new TestCase ( SECTION, "'abcdefg'.match(new RegExp('(^a)bc')); RegExp.lastParen",
	       'a', RegExp.lastParen);

// 'abcdefg'.match(/bc/); RegExp.lastParen
'abcdefg'.match(/bc/);
new TestCase ( SECTION, "'abcdefg'.match(/bc/); RegExp.lastParen",
	       '', RegExp.lastParen);

test();
