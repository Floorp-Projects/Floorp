/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     RegExp_lastParen_as_array.js
   Description:  'Tests RegExps $+ property (same tests as RegExp_lastParen.js but using $+)'

   Author:       Nick Lerissa
   Date:         March 13, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE   = 'RegExp: $+';

writeHeaderToLog('Executing script: RegExp_lastParen_as_array.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abcd'.match(/(abc)d/); RegExp['$+']
'abcd'.match(/(abc)d/);
new TestCase ( "'abcd'.match(/(abc)d/); RegExp['$+']",
	       'abc', RegExp['$+']);

// 'abcd'.match(/(bcd)e/); RegExp['$+']
'abcd'.match(/(bcd)e/);
new TestCase ( "'abcd'.match(/(bcd)e/); RegExp['$+']",
	       'abc', RegExp['$+']);

// 'abcdefg'.match(/(a(b(c(d)e)f)g)/); RegExp['$+']
'abcdefg'.match(/(a(b(c(d)e)f)g)/);
new TestCase ( "'abcdefg'.match(/(a(b(c(d)e)f)g)/); RegExp['$+']",
	       'd', RegExp['$+']);

// 'abcdefg'.match(new RegExp('(a(b(c(d)e)f)g)')); RegExp['$+']
'abcdefg'.match(new RegExp('(a(b(c(d)e)f)g)'));
new TestCase ( "'abcdefg'.match(new RegExp('(a(b(c(d)e)f)g)')); RegExp['$+']",
	       'd', RegExp['$+']);

// 'abcdefg'.match(/(a(b)c)(d(e)f)/); RegExp['$+']
'abcdefg'.match(/(a(b)c)(d(e)f)/);
new TestCase ( "'abcdefg'.match(/(a(b)c)(d(e)f)/); RegExp['$+']",
	       'e', RegExp['$+']);

// 'abcdefg'.match(/(^)abc/); RegExp['$+']
'abcdefg'.match(/(^)abc/);
new TestCase ( "'abcdefg'.match(/(^)abc/); RegExp['$+']",
	       '', RegExp['$+']);

// 'abcdefg'.match(/(^a)bc/); RegExp['$+']
'abcdefg'.match(/(^a)bc/);
new TestCase ( "'abcdefg'.match(/(^a)bc/); RegExp['$+']",
	       'a', RegExp['$+']);

// 'abcdefg'.match(new RegExp('(^a)bc')); RegExp['$+']
'abcdefg'.match(new RegExp('(^a)bc'));
new TestCase ( "'abcdefg'.match(new RegExp('(^a)bc')); RegExp['$+']",
	       'a', RegExp['$+']);

// 'abcdefg'.match(/bc/); RegExp['$+']
'abcdefg'.match(/bc/);
new TestCase ( "'abcdefg'.match(/bc/); RegExp['$+']",
	       '', RegExp['$+']);

test();
