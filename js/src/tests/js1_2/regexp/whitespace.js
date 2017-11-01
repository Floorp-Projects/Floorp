/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     whitespace.js
   Description:  'Tests regular expressions containing \f\n\r\t\v\s\S\ '

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE   = 'RegExp: \\f\\n\\r\\t\\v\\s\\S ';

writeHeaderToLog('Executing script: whitespace.js');
writeHeaderToLog( SECTION + " "+ TITLE);


var non_whitespace = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ~`!@#$%^&*()-+={[}]|\\:;'<,>./?1234567890" + '"';
var whitespace     = "\f\n\r\t\v ";

// be sure all whitespace is matched by \s
new TestCase ( "'" + whitespace + "'.match(new RegExp('\\s+'))",
	       String([whitespace]), String(whitespace.match(new RegExp('\\s+'))));

// be sure all non-whitespace is matched by \S
new TestCase ( "'" + non_whitespace + "'.match(new RegExp('\\S+'))",
	       String([non_whitespace]), String(non_whitespace.match(new RegExp('\\S+'))));

// be sure all non-whitespace is not matched by \s
new TestCase ( "'" + non_whitespace + "'.match(new RegExp('\\s'))",
	       null, non_whitespace.match(new RegExp('\\s')));

// be sure all whitespace is not matched by \S
new TestCase ( "'" + whitespace + "'.match(new RegExp('\\S'))",
	       null, whitespace.match(new RegExp('\\S')));

var s = non_whitespace + whitespace;

// be sure all digits are matched by \s
new TestCase ( "'" + s + "'.match(new RegExp('\\s+'))",
	       String([whitespace]), String(s.match(new RegExp('\\s+'))));

s = whitespace + non_whitespace;

// be sure all non-whitespace are matched by \S
new TestCase ( "'" + s + "'.match(new RegExp('\\S+'))",
	       String([non_whitespace]), String(s.match(new RegExp('\\S+'))));

// '1233345find me345'.match(new RegExp('[a-z\\s][a-z\\s]+'))
new TestCase ( "'1233345find me345'.match(new RegExp('[a-z\\s][a-z\\s]+'))",
	       String(["find me"]), String('1233345find me345'.match(new RegExp('[a-z\\s][a-z\\s]+'))));

var i;

// be sure all whitespace characters match individually
for (i = 0; i < whitespace.length; ++i)
{
  s = 'ab' + whitespace[i] + 'cd';
  new TestCase ( "'" + s + "'.match(new RegExp('\\\\s'))",
		 String([whitespace[i]]), String(s.match(new RegExp('\\s'))));
  new TestCase ( "'" + s + "'.match(/\s/)",
		 String([whitespace[i]]), String(s.match(/\s/)));
}
// be sure all non_whitespace characters match individually
for (i = 0; i < non_whitespace.length; ++i)
{
  s = '  ' + non_whitespace[i] + '  ';
  new TestCase ( "'" + s + "'.match(new RegExp('\\\\S'))",
		 String([non_whitespace[i]]), String(s.match(new RegExp('\\S'))));
  new TestCase ( "'" + s + "'.match(/\S/)",
		 String([non_whitespace[i]]), String(s.match(/\S/)));
}


test();
