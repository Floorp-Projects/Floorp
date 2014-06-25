/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     exec.js
   Description:  'Tests regular expressions exec compile'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: exec';

writeHeaderToLog('Executing script: exec.js');
writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase ( SECTION,
	       "/[0-9]{3}/.exec('23 2 34 678 9 09')",
	       String(["678"]), String(/[0-9]{3}/.exec('23 2 34 678 9 09')));

new TestCase ( SECTION,
	       "/3.{4}8/.exec('23 2 34 678 9 09')",
	       String(["34 678"]), String(/3.{4}8/.exec('23 2 34 678 9 09')));

var re = new RegExp('3.{4}8');
new TestCase ( SECTION,
	       "re.exec('23 2 34 678 9 09')",
	       String(["34 678"]), String(re.exec('23 2 34 678 9 09')));

new TestCase ( SECTION,
	       "(/3.{4}8/.exec('23 2 34 678 9 09').length",
	       1, (/3.{4}8/.exec('23 2 34 678 9 09')).length);

re = new RegExp('3.{4}8');
new TestCase ( SECTION,
	       "(re.exec('23 2 34 678 9 09').length",
	       1, (re.exec('23 2 34 678 9 09')).length);

test();
