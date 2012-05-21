/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    31 January 2003
 * SUMMARY: Testing regular expressions of form /(x|y){n,}/
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=191479
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 191479;
var summary = 'Testing regular expressions of form /(x|y){n,}/';
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
string = '12 3 45';
pattern = /(\d|\d\s){2,}/;
actualmatch = string.match(pattern);
expectedmatch = Array('12', '2');
addThis();

status = inSection(2);
string = '12 3 45';
pattern = /(\d|\d\s){4,}/;
actualmatch = string.match(pattern);
expectedmatch = Array(string, '5');
addThis();

status = inSection(3);
string = '12 3 45';
pattern = /(\d|\d\s)+/;
actualmatch = string.match(pattern);
expectedmatch = Array('12', '2');
addThis();

status = inSection(4);
string = '12 3 45';
pattern = /(\d\s?){4,}/;
actualmatch = string.match(pattern);
expectedmatch = Array(string, '5');
addThis();

/*
 * Let's reverse the operands in Sections 1-3 above -
 */
status = inSection(5);
string = '12 3 45';
pattern = /(\d\s|\d){2,}/;
actualmatch = string.match(pattern);
expectedmatch = Array(string, '5');
addThis();

status = inSection(6);
string = '12 3 45';
pattern = /(\d\s|\d){4,}/;
actualmatch = string.match(pattern);
expectedmatch = Array(string, '5');
addThis();

status = inSection(7);
string = '12 3 45';
pattern = /(\d\s|\d)+/;
actualmatch = string.match(pattern);
expectedmatch = Array(string, '5');
addThis();


/*
 * Let's take all 7 sections above and make each quantifer non-greedy.
 *
 * This is done by appending ? to it. It doesn't change the meaning of
 * the quantifier, but makes it non-greedy, which affects the results -
 */
status = inSection(8);
string = '12 3 45';
pattern = /(\d|\d\s){2,}?/;
actualmatch = string.match(pattern);
expectedmatch = Array('12', '2');
addThis();

status = inSection(9);
string = '12 3 45';
pattern = /(\d|\d\s){4,}?/;
actualmatch = string.match(pattern);
expectedmatch = Array('12 3 4', '4');
addThis();

status = inSection(10);
string = '12 3 45';
pattern = /(\d|\d\s)+?/;
actualmatch = string.match(pattern);
expectedmatch = Array('1', '1');
addThis();

status = inSection(11);
string = '12 3 45';
pattern = /(\d\s?){4,}?/;
actualmatch = string.match(pattern);
expectedmatch = Array('12 3 4', '4');
addThis();

status = inSection(12);
string = '12 3 45';
pattern = /(\d\s|\d){2,}?/;
actualmatch = string.match(pattern);
expectedmatch = Array('12 ', '2 ');
addThis();

status = inSection(13);
string = '12 3 45';
pattern = /(\d\s|\d){4,}?/;
actualmatch = string.match(pattern);
expectedmatch = Array('12 3 4', '4');
addThis();

status = inSection(14);
string = '12 3 45';
pattern = /(\d\s|\d)+?/;
actualmatch = string.match(pattern);
expectedmatch = Array('1', '1');
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  testRegExp(statusmessages, patterns, strings, actualmatches, expectedmatches);
  exitFunc ('test');
}
