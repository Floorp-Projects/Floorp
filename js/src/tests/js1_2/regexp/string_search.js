/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     string_search.js
   Description:  'Tests the search method on Strings using regular expressions'

   Author:       Nick Lerissa
   Date:         March 12, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE   = 'String: search';

writeHeaderToLog('Executing script: string_search.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abcdefg'.search(/d/)
new TestCase ( "'abcdefg'.search(/d/)",
	       3, 'abcdefg'.search(/d/));

// 'abcdefg'.search(/x/)
new TestCase ( "'abcdefg'.search(/x/)",
	       -1, 'abcdefg'.search(/x/));

// 'abcdefg123456hijklmn'.search(/\d+/)
new TestCase ( "'abcdefg123456hijklmn'.search(/\d+/)",
	       7, 'abcdefg123456hijklmn'.search(/\d+/));

// 'abcdefg123456hijklmn'.search(new RegExp())
new TestCase ( "'abcdefg123456hijklmn'.search(new RegExp())",
	       0, 'abcdefg123456hijklmn'.search(new RegExp()));

// 'abc'.search(new RegExp('$'))
new TestCase ( "'abc'.search(new RegExp('$'))",
	       3, 'abc'.search(new RegExp('$')));

// 'abc'.search(new RegExp('^'))
new TestCase ( "'abc'.search(new RegExp('^'))",
	       0, 'abc'.search(new RegExp('^')));

// 'abc1'.search(/.\d/)
new TestCase ( "'abc1'.search(/.\d/)",
	       2, 'abc1'.search(/.\d/));

// 'abc1'.search(/\d{2}/)
new TestCase ( "'abc1'.search(/\d{2}/)",
	       -1, 'abc1'.search(/\d{2}/));

test();
