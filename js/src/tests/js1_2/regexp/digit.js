/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     digit.js
   Description:  'Tests regular expressions containing \d'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: \\d';

writeHeaderToLog('Executing script: digit.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var non_digits = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\f\n\r\t\v~`!@#$%^&*()-+={[}]|\\:;'<,>./? " + '"';

var digits = "1234567890";

// be sure all digits are matched by \d
new TestCase ( SECTION,
	       "'" + digits + "'.match(new RegExp('\\d+'))",
	       String([digits]), String(digits.match(new RegExp('\\d+'))));

// be sure all non-digits are matched by \D
new TestCase ( SECTION,
	       "'" + non_digits + "'.match(new RegExp('\\D+'))",
	       String([non_digits]), String(non_digits.match(new RegExp('\\D+'))));

// be sure all non-digits are not matched by \d
new TestCase ( SECTION,
	       "'" + non_digits + "'.match(new RegExp('\\d'))",
	       null, non_digits.match(new RegExp('\\d')));

// be sure all digits are not matched by \D
new TestCase ( SECTION,
	       "'" + digits + "'.match(new RegExp('\\D'))",
	       null, digits.match(new RegExp('\\D')));

var s = non_digits + digits;

// be sure all digits are matched by \d
new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\d+'))",
	       String([digits]), String(s.match(new RegExp('\\d+'))));

var s = digits + non_digits;

// be sure all non-digits are matched by \D
new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\D+'))",
	       String([non_digits]), String(s.match(new RegExp('\\D+'))));

var i;

// be sure all digits match individually
for (i = 0; i < digits.length; ++i)
{
  s = 'ab' + digits[i] + 'cd';
  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\d'))",
		 String([digits[i]]), String(s.match(new RegExp('\\d'))));
  new TestCase ( SECTION,
		 "'" + s + "'.match(/\\d/)",
		 String([digits[i]]), String(s.match(/\d/)));
}
// be sure all non_digits match individually
for (i = 0; i < non_digits.length; ++i)
{
  s = '12' + non_digits[i] + '34';
  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\D'))",
		 String([non_digits[i]]), String(s.match(new RegExp('\\D'))));
  new TestCase ( SECTION,
		 "'" + s + "'.match(/\\D/)",
		 String([non_digits[i]]), String(s.match(/\D/)));
}

test();
