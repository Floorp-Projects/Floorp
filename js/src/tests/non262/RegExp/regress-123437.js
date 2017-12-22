/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    04 Feb 2002
 * SUMMARY: regexp backreferences must hold |undefined| if not used
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=123437 (SpiderMonkey)
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=123439 (Rhino)
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 123437;
var summary = 'regexp backreferences must hold |undefined| if not used';
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


pattern = /(a)?a/;
string = 'a';
status = inSection(1);
actualmatch = string.match(pattern);
expectedmatch = Array('a', undefined);
addThis();

pattern = /a|(b)/;
string = 'a';
status = inSection(2);
actualmatch = string.match(pattern);
expectedmatch = Array('a', undefined);
addThis();

pattern = /(a)?(a)/;
string = 'a';
status = inSection(3);
actualmatch = string.match(pattern);
expectedmatch = Array('a', undefined, 'a');
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
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  testRegExp(statusmessages, patterns, strings, actualmatches, expectedmatches);
}
