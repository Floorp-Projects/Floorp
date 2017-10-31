/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     backspace.js
   Description:  'Tests regular expressions containing [\b]'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE = 'RegExp: [\b]';

writeHeaderToLog('Executing script: backspace.js');
writeHeaderToLog( SECTION + " "+ TITLE);

// 'abc\bdef'.match(new RegExp('.[\b].'))
new TestCase ( "'abc\bdef'.match(new RegExp('.[\\b].'))",
	       String(["c\bd"]), String('abc\bdef'.match(new RegExp('.[\\b].'))));

// 'abc\\bdef'.match(new RegExp('.[\b].'))
new TestCase ( "'abc\\bdef'.match(new RegExp('.[\\b].'))",
	       null, 'abc\\bdef'.match(new RegExp('.[\\b].')));

// 'abc\b\b\bdef'.match(new RegExp('c[\b]{3}d'))
new TestCase ( "'abc\b\b\bdef'.match(new RegExp('c[\\b]{3}d'))",
	       String(["c\b\b\bd"]), String('abc\b\b\bdef'.match(new RegExp('c[\\b]{3}d'))));

// 'abc\bdef'.match(new RegExp('[^\\[\b\\]]+'))
new TestCase ( "'abc\bdef'.match(new RegExp('[^\\[\\b\\]]+'))",
	       String(["abc"]), String('abc\bdef'.match(new RegExp('[^\\[\\b\\]]+'))));

// 'abcdef'.match(new RegExp('[^\\[\b\\]]+'))
new TestCase ( "'abcdef'.match(new RegExp('[^\\[\\b\\]]+'))",
	       String(["abcdef"]), String('abcdef'.match(new RegExp('[^\\[\\b\\]]+'))));

// 'abcdef'.match(/[^\[\b\]]+/)
new TestCase ( "'abcdef'.match(/[^\\[\\b\\]]+/)",
	       String(["abcdef"]), String('abcdef'.match(/[^\[\b\]]+/)));

test();
