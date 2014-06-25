/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     RegExp_lastMatch.js
   Description:  'Tests RegExps lastMatch property'

   Author:       Nick Lerissa
   Date:         March 12, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: lastMatch';

writeHeaderToLog('Executing script: RegExp_lastMatch.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'foo'.match(/foo/); RegExp.lastMatch
'foo'.match(/foo/);
new TestCase ( SECTION, "'foo'.match(/foo/); RegExp.lastMatch",
	       'foo', RegExp.lastMatch);

// 'foo'.match(new RegExp('foo')); RegExp.lastMatch
'foo'.match(new RegExp('foo'));
new TestCase ( SECTION, "'foo'.match(new RegExp('foo')); RegExp.lastMatch",
	       'foo', RegExp.lastMatch);

// 'xxx'.match(/bar/); RegExp.lastMatch
'xxx'.match(/bar/);
new TestCase ( SECTION, "'xxx'.match(/bar/); RegExp.lastMatch",
	       'foo', RegExp.lastMatch);

// 'xxx'.match(/$/); RegExp.lastMatch
'xxx'.match(/$/);
new TestCase ( SECTION, "'xxx'.match(/$/); RegExp.lastMatch",
	       '', RegExp.lastMatch);

// 'abcdefg'.match(/^..(cd)[a-z]+/); RegExp.lastMatch
'abcdefg'.match(/^..(cd)[a-z]+/);
new TestCase ( SECTION, "'abcdefg'.match(/^..(cd)[a-z]+/); RegExp.lastMatch",
	       'abcdefg', RegExp.lastMatch);

// 'abcdefgabcdefg'.match(/(a(b(c(d)e)f)g)\1/); RegExp.lastMatch
'abcdefgabcdefg'.match(/(a(b(c(d)e)f)g)\1/);
new TestCase ( SECTION, "'abcdefgabcdefg'.match(/(a(b(c(d)e)f)g)\\1/); RegExp.lastMatch",
	       'abcdefgabcdefg', RegExp.lastMatch);

test();
