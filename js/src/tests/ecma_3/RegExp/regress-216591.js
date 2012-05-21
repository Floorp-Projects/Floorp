/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    19 August 2003
 * SUMMARY: Regexp conformance test
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=216591
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 216591;
var summary = 'Regexp conformance test';
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
string = 'a {result.data.DATA} b';
pattern = /\{(([a-z0-9\-_]+?\.)+?)([a-z0-9\-_]+?)\}/i;
actualmatch = string.match(pattern);
expectedmatch = Array('{result.data.DATA}', 'result.data.', 'data.', 'DATA');
addThis();

/*
 * Add a global flag to the regexp. In Perl 5, this gives the same results as above. Compare:
 *
 * [ ] perl -e '"a {result.data.DATA} b" =~ /\{(([a-z0-9\-_]+?\.)+?)([a-z0-9\-_]+?)\}/i;  print("$&, $1, $2, $3");'
 * {result.data.DATA}, result.data., data., DATA
 *
 * [ ] perl -e '"a {result.data.DATA} b" =~ /\{(([a-z0-9\-_]+?\.)+?)([a-z0-9\-_]+?)\}/gi; print("$&, $1, $2, $3");'
 * {result.data.DATA}, result.data., data., DATA
 *
 *
 * But in JavaScript, there will no longer be any sub-captures:
 */
status = inSection(2);
string = 'a {result.data.DATA} b';
pattern = /\{(([a-z0-9\-_]+?\.)+?)([a-z0-9\-_]+?)\}/gi;
actualmatch = string.match(pattern);
expectedmatch = Array('{result.data.DATA}');
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
