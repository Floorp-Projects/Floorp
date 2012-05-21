/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 01 May 2001
 *
 * SUMMARY:  Regression test for Bugzilla bug 31316:
 * "Rhino: Regexp matches return garbage"
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=31316
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 31316;
var summary = 'Regression test for Bugzilla bug 31316';
var cnEmptyString = '';
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
pattern = /<([^\/<>][^<>]*[^\/])>|<([^\/<>])>/;
string = '<p>Some<br />test</p>';
actualmatch = string.match(pattern);
expectedmatch = Array('<p>', undefined, 'p');
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
