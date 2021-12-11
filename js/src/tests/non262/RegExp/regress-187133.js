/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    06 January 2003
 * SUMMARY: RegExp conformance test
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=187133
 *
 * The tests here employ the regular expression construct:
 *
 *                   (?!pattern)
 *
 * This is a "zero-width lookahead negative assertion".
 * From the Perl documentation:
 *
 *   For example, /foo(?!bar)/ matches any occurrence
 *   of 'foo' that isn't followed by 'bar'.
 *
 * It is "zero-width" means that it does not consume any characters and that
 * the parens are non-capturing. A non-null match array in the example above
 * will have only have length 1, not 2.
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 187133;
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


pattern = /(\.(?!com|org)|\/)/;
status = inSection(1);
string = 'ah.info';
actualmatch = string.match(pattern);
expectedmatch = ['.', '.'];
addThis();

status = inSection(2);
string = 'ah/info';
actualmatch = string.match(pattern);
expectedmatch = ['/', '/'];
addThis();

status = inSection(3);
string = 'ah.com';
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();


pattern = /(?!a|b)|c/;
status = inSection(4);
string = '';
actualmatch = string.match(pattern);
expectedmatch = [''];
addThis();

status = inSection(5);
string = 'bc';
actualmatch = string.match(pattern);
expectedmatch = [''];
addThis();

status = inSection(6);
string = 'd';
actualmatch = string.match(pattern);
expectedmatch = [''];
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
