/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     vertical_bar.js
   Description:  'Tests regular expressions containing |'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE   = 'RegExp: |';

writeHeaderToLog('Executing script: vertical_bar.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'abc'.match(new RegExp('xyz|abc'))
new TestCase ( "'abc'.match(new RegExp('xyz|abc'))",
	       String(["abc"]), String('abc'.match(new RegExp('xyz|abc'))));

// 'this is a test'.match(new RegExp('quiz|exam|test|homework'))
new TestCase ( "'this is a test'.match(new RegExp('quiz|exam|test|homework'))",
	       String(["test"]), String('this is a test'.match(new RegExp('quiz|exam|test|homework'))));

// 'abc'.match(new RegExp('xyz|...'))
new TestCase ( "'abc'.match(new RegExp('xyz|...'))",
	       String(["abc"]), String('abc'.match(new RegExp('xyz|...'))));

// 'abc'.match(new RegExp('(.)..|abc'))
new TestCase ( "'abc'.match(new RegExp('(.)..|abc'))",
	       String(["abc","a"]), String('abc'.match(new RegExp('(.)..|abc'))));

// 'color: grey'.match(new RegExp('.+: gr(a|e)y'))
new TestCase ( "'color: grey'.match(new RegExp('.+: gr(a|e)y'))",
	       String(["color: grey","e"]), String('color: grey'.match(new RegExp('.+: gr(a|e)y'))));

// 'no match'.match(new RegExp('red|white|blue'))
new TestCase ( "'no match'.match(new RegExp('red|white|blue'))",
	       null, 'no match'.match(new RegExp('red|white|blue')));

// 'Hi Bob'.match(new RegExp('(Rob)|(Bob)|(Robert)|(Bobby)'))
new TestCase ( "'Hi Bob'.match(new RegExp('(Rob)|(Bob)|(Robert)|(Bobby)'))",
	       String(["Bob",undefined,"Bob", undefined, undefined]), String('Hi Bob'.match(new RegExp('(Rob)|(Bob)|(Robert)|(Bobby)'))));

// 'abcdef'.match(new RegExp('abc|bcd|cde|def'))
new TestCase ( "'abcdef'.match(new RegExp('abc|bcd|cde|def'))",
	       String(["abc"]), String('abcdef'.match(new RegExp('abc|bcd|cde|def'))));

// 'Hi Bob'.match(/(Rob)|(Bob)|(Robert)|(Bobby)/)
new TestCase ( "'Hi Bob'.match(/(Rob)|(Bob)|(Robert)|(Bobby)/)",
	       String(["Bob",undefined,"Bob", undefined, undefined]), String('Hi Bob'.match(/(Rob)|(Bob)|(Robert)|(Bobby)/)));

// 'abcdef'.match(/abc|bcd|cde|def/)
new TestCase ( "'abcdef'.match(/abc|bcd|cde|def/)",
	       String(["abc"]), String('abcdef'.match(/abc|bcd|cde|def/)));

test();
