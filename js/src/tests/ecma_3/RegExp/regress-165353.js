/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    31 August 2002
 * SUMMARY: RegExp conformance test
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=165353
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 165353;
var summary = 'RegExp conformance test';
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


pattern = /^([a-z]+)*[a-z]$/;
status = inSection(1);
string = 'a';
actualmatch = string.match(pattern);
expectedmatch = Array('a', undefined);
addThis();

status = inSection(2);
string = 'ab';
actualmatch = string.match(pattern);
expectedmatch = Array('ab', 'a');
addThis();

status = inSection(3);
string = 'abc';
actualmatch = string.match(pattern);
expectedmatch = Array('abc', 'ab');
addThis();


string = 'www.netscape.com';
status = inSection(4);
pattern = /^(([a-z]+)*[a-z]\.)+[a-z]{2,}$/;
actualmatch = string.match(pattern);
expectedmatch = Array('www.netscape.com', 'netscape.', 'netscap');
addThis();

// add one more capturing parens to the previous regexp -
status = inSection(5);
pattern = /^(([a-z]+)*([a-z])\.)+[a-z]{2,}$/;
actualmatch = string.match(pattern);
expectedmatch = Array('www.netscape.com', 'netscape.', 'netscap', 'e');
addThis();



//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


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
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  testRegExp(statusmessages, patterns, strings, actualmatches, expectedmatches);
}
