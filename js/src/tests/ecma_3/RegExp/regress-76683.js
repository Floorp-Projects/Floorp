/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 01 May 2001
 *
 * SUMMARY: Regression test for Bugzilla bug 76683 on Rhino:
 * "RegExp regression (NullPointerException)"
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=76683
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 76683;
var summary = 'Regression test for Bugzilla bug 76683';
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
 * Rhino (2001-04-19) crashed on the 3rd regular expression below.
 * It didn't matter what the string was. No problem in SpiderMonkey -
 */
string = 'abc';
status = inSection(1);
pattern = /(<!--([^-]|-[^-]|--[^>])*-->)|(<([\$\w:\.\-]+)((([ ][^\/>]*)?\/>)|(([ ][^>]*)?>)))/;
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();

status = inSection(2);
pattern = /(<!--([^-]|-[^-]|--[^>])*-->)|(<(tagPattern)((([ ][^\/>]*)?\/>)|(([ ][^>]*)?>)))/;
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();

// This was the one causing a Rhino crash -
status = inSection(3);
pattern = /(<!--([^-]|-[^-]|--[^>])*-->)|(<(tagPattern)((([ ][^\/>]*)?\/>)|(([ ][^>]*)?>)))|(<\/tagPattern[^>]*>)/;
actualmatch = string.match(pattern);
expectedmatch = null;
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
