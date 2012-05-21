/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 22 June 2001
 *
 * SUMMARY:  Regression test for Bugzilla bug 87231:
 * "Regular expression /(A)?(A.*)/ picks 'A' twice"
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=87231
 * Key case:
 *
 *            pattern = /^(A)?(A.*)$/;
 *            string = 'A';
 *            expectedmatch = Array('A', '', 'A');
 *
 *
 * We expect the 1st subexpression (A)? NOT to consume the single 'A'.
 * Recall that "?" means "match 0 or 1 times". Here, it should NOT do
 * greedy matching: it should match 0 times instead of 1. This allows
 * the 2nd subexpression to make the only match it can: the single 'A'.
 * Such "altruism" is the only way there can be a successful global match...
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 87231;
var cnEmptyString = '';
var summary = 'Testing regular expression /(A)?(A.*)/';
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


pattern = /^(A)?(A.*)$/;
status = inSection(1);
string = 'AAA';
actualmatch = string.match(pattern);
expectedmatch = Array('AAA', 'A', 'AA');
addThis();

status = inSection(2);
string = 'AA';
actualmatch = string.match(pattern);
expectedmatch = Array('AA', 'A', 'A');
addThis();

status = inSection(3);
string = 'A';
actualmatch = string.match(pattern);
expectedmatch = Array('A', undefined, 'A'); // 'altruistic' case: see above
addThis();


pattern = /(A)?(A.*)/;
var strL = 'zxcasd;fl\\\  ^';
var strR = 'aaAAaaaf;lrlrzs';

status = inSection(4);
string =  strL + 'AAA' + strR;
actualmatch = string.match(pattern);
expectedmatch = Array('AAA' + strR, 'A', 'AA' + strR);
addThis();

status = inSection(5);
string =  strL + 'AA' + strR;
actualmatch = string.match(pattern);
expectedmatch = Array('AA' + strR, 'A', 'A' + strR);
addThis();

status = inSection(6);
string =  strL + 'A' + strR;
actualmatch = string.match(pattern);
expectedmatch = Array('A' + strR, undefined, 'A' + strR); // 'altruistic' case: see above
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
