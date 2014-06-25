/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    24 October 2003
 * SUMMARY: Testing regexps with empty alternatives
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=223535
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 223535;
var summary = 'Testing regexps with empty alternatives';
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


string = 'a';
status = inSection(1);
pattern = /a|/;
actualmatch = string.match(pattern);
expectedmatch = Array('a');
addThis();

status = inSection(2);
pattern = /|a/;
actualmatch = string.match(pattern);
expectedmatch = Array('');
addThis();

status = inSection(3);
pattern = /|/;
actualmatch = string.match(pattern);
expectedmatch = Array('');
addThis();

status = inSection(4);
pattern = /(a|)/;
actualmatch = string.match(pattern);
expectedmatch = Array('a', 'a');
addThis();

status = inSection(5);
pattern = /(a||)/;
actualmatch = string.match(pattern);
expectedmatch = Array('a', 'a');
addThis();

status = inSection(6);
pattern = /(|a)/;
actualmatch = string.match(pattern);
expectedmatch = Array('', '');
addThis();

status = inSection(7);
pattern = /(|a|)/;
actualmatch = string.match(pattern);
expectedmatch = Array('', '');
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
