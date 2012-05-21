/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    10 November 2003
 * SUMMARY: Testing regexps with complementary alternatives
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=225289
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 225289;
var summary = 'Testing regexps with complementary alternatives';
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


// this pattern should match any string!
pattern = /a|[^a]/;

status = inSection(1);
string = 'a';
actualmatch = string.match(pattern);
expectedmatch = Array('a');
addThis();

status = inSection(2);
string = '';
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();

status = inSection(3);
string = '()';
actualmatch = string.match(pattern);
expectedmatch = Array('(');
addThis();


pattern = /(a|[^a])/;

status = inSection(4);
string = 'a';
actualmatch = string.match(pattern);
expectedmatch = Array('a', 'a');
addThis();

status = inSection(5);
string = '';
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();

status = inSection(6);
string = '()';
actualmatch = string.match(pattern);
expectedmatch = Array('(', '(');
addThis();


pattern = /(a)|([^a])/;

status = inSection(7);
string = 'a';
actualmatch = string.match(pattern);
expectedmatch = Array('a', 'a', undefined);
addThis();

status = inSection(8);
string = '';
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();

status = inSection(9);
string = '()';
actualmatch = string.match(pattern);
expectedmatch = Array('(', undefined, '(');
addThis();


// note this pattern has one non-capturing parens
pattern = /((?:a|[^a])*)/g;

status = inSection(10);
string = 'a';
actualmatch = string.match(pattern);
expectedmatch = Array('a', ''); // see bug 225289 comment 6
addThis();

status = inSection(11);
string = '';
actualmatch = string.match(pattern);
expectedmatch = Array(''); // see bug 225289 comment 9
addThis();

status = inSection(12);
string = '()';
actualmatch = string.match(pattern);
expectedmatch = Array('()', ''); // see bug 225289 comment 6
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  testRegExp(statusmessages, patterns, strings, actualmatches, expectedmatches);
  exitFunc ('test');
}
