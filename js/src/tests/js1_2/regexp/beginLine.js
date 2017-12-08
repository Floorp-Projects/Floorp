/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     beginLine.js
   Description:  'Tests regular expressions containing ^'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE = 'RegExp: ^';

writeHeaderToLog('Executing script: beginLine.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'abcde'.match(new RegExp('^ab'))
new TestCase ( "'abcde'.match(new RegExp('^ab'))",
	       String(["ab"]), String('abcde'.match(new RegExp('^ab'))));

// 'ab\ncde'.match(new RegExp('^..^e'))
new TestCase ( "'ab\ncde'.match(new RegExp('^..^e'))",
	       null, 'ab\ncde'.match(new RegExp('^..^e')));

// 'yyyyy'.match(new RegExp('^xxx'))
new TestCase ( "'yyyyy'.match(new RegExp('^xxx'))",
	       null, 'yyyyy'.match(new RegExp('^xxx')));

// '^^^x'.match(new RegExp('^\\^+'))
new TestCase ( "'^^^x'.match(new RegExp('^\\^+'))",
	       String(['^^^']), String('^^^x'.match(new RegExp('^\\^+'))));

// '^^^x'.match(/^\^+/)
new TestCase ( "'^^^x'.match(/^\\^+/)",
	       String(['^^^']), String('^^^x'.match(/^\^+/)));

test();
