/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 06 February 2001
 *
 * SUMMARY:  Arose from Bugzilla bug 78156:
 * "m flag of regular expression does not work with $"
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=78156
 *
 * The m flag means a regular expression should search strings
 * across multiple lines, i.e. across '\n', '\r'.
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 78156;
var summary = 'Testing regular expressions with  ^, $, and the m flag -';
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

/*
 * All patterns have an m flag; all strings are multiline.
 * Looking for digit characters at beginning/end of lines.
 */

string = 'aaa\n789\r\nccc\r\n345';
status = inSection(1);
pattern = /^\d/gm;
actualmatch = string.match(pattern);
expectedmatch = ['7','3'];
addThis();

status = inSection(2);
pattern = /\d$/gm;
actualmatch = string.match(pattern);
expectedmatch = ['9','5'];
addThis();

string = 'aaa\n789\r\nccc\r\nddd';
status = inSection(3);
pattern = /^\d/gm;
actualmatch = string.match(pattern);
expectedmatch = ['7'];
addThis();

status = inSection(4);
pattern = /\d$/gm;
actualmatch = string.match(pattern);
expectedmatch = ['9'];
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
