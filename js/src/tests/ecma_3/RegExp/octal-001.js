/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    18 July 2002
 * SUMMARY: Testing octal sequences in regexps
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=141078
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 141078;
var summary = 'Testing octal sequences in regexps';
var status = '';
var statusmessages = new Array();
var pattern = '';
var patterns = new Array();
var string = '';
var strings = new Array();
var actualmatch = '';
var actualmatches = new Array();
var expectedmatch = '';
var expectedmatches = new Array();


status = inSection(1);
pattern = /\240/;
string = 'abc';
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();

/*
 * In the following sections, we test the octal escape sequence '\052'.
 * This is character code 42, representing the asterisk character '*'.
 * The Unicode escape for it would be '\u002A', the hex escape '\x2A'.
 */
status = inSection(2);
pattern = /ab\052c/;
string = 'ab*c';
actualmatch = string.match(pattern);
expectedmatch = Array('ab*c');
addThis();

status = inSection(3);
pattern = /ab\052*c/;
string = 'abc';
actualmatch = string.match(pattern);
expectedmatch = Array('abc');
addThis();

status = inSection(4);
pattern = /ab(\052)+c/;
string = 'ab****c';
actualmatch = string.match(pattern);
expectedmatch = Array('ab****c', '*');
addThis();

status = inSection(5);
pattern = /ab((\052)+)c/;
string = 'ab****c';
actualmatch = string.match(pattern);
expectedmatch = Array('ab****c', '****', '*');
addThis();

status = inSection(6);
pattern = /(?:\052)c/;
string = 'ab****c';
actualmatch = string.match(pattern);
expectedmatch = Array('*c');
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusmessages[i] = status;
  patterns[i] = pattern;
  strings[i] = string;
  actualmatches[i] = actualmatch;
  expectedmatches[i] = expectedmatch;
  i++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);
  testRegExp(statusmessages, patterns, strings, actualmatches, expectedmatches);
  exitFunc ('test');
}
