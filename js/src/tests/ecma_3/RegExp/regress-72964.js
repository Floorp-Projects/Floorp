/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 2001-07-17
 *
 * SUMMARY: Regression test for Bugzilla bug 72964:
 * "String method for pattern matching failed for Chinese Simplified (GB2312)"
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=72964
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 72964;
var summary = 'Testing regular expressions containing non-Latin1 characters';
var cnSingleSpace = ' ';
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


pattern = /[\S]+/;
// 4 low Unicode chars = Latin1; whole string should match
status = inSection(1);
string = '\u00BF\u00CD\u00BB\u00A7';
actualmatch = string.match(pattern);
expectedmatch = Array(string);
addThis();

// Now put a space in the middle; first half of string should match
status = inSection(2);
string = '\u00BF\u00CD \u00BB\u00A7';
actualmatch = string.match(pattern);
expectedmatch = Array('\u00BF\u00CD');
addThis();


// 4 high Unicode chars = non-Latin1; whole string should match
status = inSection(3);
string = '\u4e00\uac00\u4e03\u4e00';
actualmatch = string.match(pattern);
expectedmatch = Array(string);
addThis();

// Now put a space in the middle; first half of string should match
status = inSection(4);
string = '\u4e00\uac00 \u4e03\u4e00';
actualmatch = string.match(pattern);
expectedmatch = Array('\u4e00\uac00');
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
