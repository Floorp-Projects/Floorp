/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     word_boundary.js
   Description:  'Tests regular expressions containing \b and \B'

   Author:       Nick Lerissa
   Date:         March 10, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE   = 'RegExp: \\b and \\B';

writeHeaderToLog('Executing script: word_boundary.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'cowboy boyish boy'.match(new RegExp('\bboy\b'))
new TestCase ( "'cowboy boyish boy'.match(new RegExp('\\bboy\\b'))",
	       String(["boy"]), String('cowboy boyish boy'.match(new RegExp('\\bboy\\b'))));

var boundary_characters = "\f\n\r\t\v~`!@#$%^&*()-+={[}]|\\:;'<,>./? " + '"';
var non_boundary_characters = '1234567890_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
var s     = '';
var i;

// testing whether all boundary characters are matched when they should be
for (i = 0; i < boundary_characters.length; ++i)
{
  s = '123ab' + boundary_characters.charAt(i) + '123c' + boundary_characters.charAt(i);

  new TestCase ( "'" + s + "'.match(new RegExp('\\b123[a-z]\\b'))",
		 String(["123c"]), String(s.match(new RegExp('\\b123[a-z]\\b'))));
}

// testing whether all non-boundary characters are matched when they should be
for (i = 0; i < non_boundary_characters.length; ++i)
{
  s = '123ab' + non_boundary_characters.charAt(i) + '123c' + non_boundary_characters.charAt(i);

  new TestCase ( "'" + s + "'.match(new RegExp('\\B123[a-z]\\B'))",
		 String(["123c"]), String(s.match(new RegExp('\\B123[a-z]\\B'))));
}

s = '';

// testing whether all boundary characters are not matched when they should not be
for (i = 0; i < boundary_characters.length; ++i)
{
  s += boundary_characters[i] + "a" + i + "b";
}
s += "xa1111bx";

new TestCase ( "'" + s + "'.match(new RegExp('\\Ba\\d+b\\B'))",
	       String(["a1111b"]), String(s.match(new RegExp('\\Ba\\d+b\\B'))));

new TestCase ( "'" + s + "'.match(/\\Ba\\d+b\\B/)",
	       String(["a1111b"]), String(s.match(/\Ba\d+b\B/)));

s = '';

// testing whether all non-boundary characters are not matched when they should not be
for (i = 0; i < non_boundary_characters.length; ++i)
{
  s += non_boundary_characters[i] + "a" + i + "b";
}
s += "(a1111b)";

new TestCase ( "'" + s + "'.match(new RegExp('\\ba\\d+b\\b'))",
	       String(["a1111b"]), String(s.match(new RegExp('\\ba\\d+b\\b'))));

new TestCase ( "'" + s + "'.match(/\\ba\\d+b\\b/)",
	       String(["a1111b"]), String(s.match(/\ba\d+b\b/)));

test();
