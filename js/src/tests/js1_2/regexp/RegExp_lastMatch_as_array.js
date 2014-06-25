/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     RegExp_lastMatch_as_array.js
   Description:  'Tests RegExps $& property (same tests as RegExp_lastMatch.js but using $&)'

   Author:       Nick Lerissa
   Date:         March 13, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: $&';

writeHeaderToLog('Executing script: RegExp_lastMatch_as_array.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'foo'.match(/foo/); RegExp['$&']
'foo'.match(/foo/);
new TestCase ( SECTION, "'foo'.match(/foo/); RegExp['$&']",
	       'foo', RegExp['$&']);

// 'foo'.match(new RegExp('foo')); RegExp['$&']
'foo'.match(new RegExp('foo'));
new TestCase ( SECTION, "'foo'.match(new RegExp('foo')); RegExp['$&']",
	       'foo', RegExp['$&']);

// 'xxx'.match(/bar/); RegExp['$&']
'xxx'.match(/bar/);
new TestCase ( SECTION, "'xxx'.match(/bar/); RegExp['$&']",
	       'foo', RegExp['$&']);

// 'xxx'.match(/$/); RegExp['$&']
'xxx'.match(/$/);
new TestCase ( SECTION, "'xxx'.match(/$/); RegExp['$&']",
	       '', RegExp['$&']);

// 'abcdefg'.match(/^..(cd)[a-z]+/); RegExp['$&']
'abcdefg'.match(/^..(cd)[a-z]+/);
new TestCase ( SECTION, "'abcdefg'.match(/^..(cd)[a-z]+/); RegExp['$&']",
	       'abcdefg', RegExp['$&']);

// 'abcdefgabcdefg'.match(/(a(b(c(d)e)f)g)\1/); RegExp['$&']
'abcdefgabcdefg'.match(/(a(b(c(d)e)f)g)\1/);
new TestCase ( SECTION, "'abcdefgabcdefg'.match(/(a(b(c(d)e)f)g)\\1/); RegExp['$&']",
	       'abcdefgabcdefg', RegExp['$&']);

test();
