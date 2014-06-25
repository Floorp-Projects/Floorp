/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     alphanumeric.js
   Description:  'Tests regular expressions with \w and \W special characters'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp: \\w and \\W';

writeHeaderToLog('Executing script: alphanumeric.js');
writeHeaderToLog( SECTION + " " + TITLE);

var non_alphanumeric = "~`!@#$%^&*()-+={[}]|\\:;'<,>./?\f\n\r\t\v " + '"';
var alphanumeric     = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

// be sure all alphanumerics are matched by \w
new TestCase ( SECTION,
	       "'" + alphanumeric + "'.match(new RegExp('\\w+'))",
	       String([alphanumeric]), String(alphanumeric.match(new RegExp('\\w+'))));

// be sure all non-alphanumerics are matched by \W
new TestCase ( SECTION,
	       "'" + non_alphanumeric + "'.match(new RegExp('\\W+'))",
	       String([non_alphanumeric]), String(non_alphanumeric.match(new RegExp('\\W+'))));

// be sure all non-alphanumerics are not matched by \w
new TestCase ( SECTION,
	       "'" + non_alphanumeric + "'.match(new RegExp('\\w'))",
	       null, non_alphanumeric.match(new RegExp('\\w')));

// be sure all alphanumerics are not matched by \W
new TestCase ( SECTION,
	       "'" + alphanumeric + "'.match(new RegExp('\\W'))",
	       null, alphanumeric.match(new RegExp('\\W')));

var s = non_alphanumeric + alphanumeric;

// be sure all alphanumerics are matched by \w
new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\w+'))",
	       String([alphanumeric]), String(s.match(new RegExp('\\w+'))));

s = alphanumeric + non_alphanumeric;

// be sure all non-alphanumerics are matched by \W
new TestCase ( SECTION,
	       "'" + s + "'.match(new RegExp('\\W+'))",
	       String([non_alphanumeric]), String(s.match(new RegExp('\\W+'))));

// be sure all alphanumerics are matched by \w (using literals)
new TestCase ( SECTION,
	       "'" + s + "'.match(/\w+/)",
	       String([alphanumeric]), String(s.match(/\w+/)));

s = alphanumeric + non_alphanumeric;

// be sure all non-alphanumerics are matched by \W (using literals)
new TestCase ( SECTION,
	       "'" + s + "'.match(/\W+/)",
	       String([non_alphanumeric]), String(s.match(/\W+/)));

s = 'abcd*&^%$$';
// be sure the following test behaves consistently
new TestCase ( SECTION,
	       "'" + s + "'.match(/(\w+)...(\W+)/)",
	       String([s , 'abcd' , '%$$']), String(s.match(/(\w+)...(\W+)/)));

var i;

// be sure all alphanumeric characters match individually
for (i = 0; i < alphanumeric.length; ++i)
{
  s = '#$' + alphanumeric[i] + '%^';
  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\w'))",
		 String([alphanumeric[i]]), String(s.match(new RegExp('\\w'))));
}
// be sure all non_alphanumeric characters match individually
for (i = 0; i < non_alphanumeric.length; ++i)
{
  s = 'sd' + non_alphanumeric[i] + String((i+10) * (i+10) - 2 * (i+10));
  new TestCase ( SECTION,
		 "'" + s + "'.match(new RegExp('\\W'))",
		 String([non_alphanumeric[i]]), String(s.match(new RegExp('\\W'))));
}

test();
