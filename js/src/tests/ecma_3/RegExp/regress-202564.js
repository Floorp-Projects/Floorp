/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    18 April 2003
 * SUMMARY: Testing regexp with many backreferences
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=202564
 *
 * Note that in Section 1 below, we expect the 1st and 4th backreferences
 * to hold |undefined| instead of the empty strings one gets in Perl and IE6.
 * This is because per ECMA, regexp backreferences must hold |undefined|
 * if not used. See http://bugzilla.mozilla.org/show_bug.cgi?id=123437.
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 202564;
var summary = 'Testing regexp with many backreferences';
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
string = 'Seattle, WA to Buckley, WA';
pattern = /(?:(.+), )?(.+), (..) to (?:(.+), )?(.+), (..)/;
actualmatch = string.match(pattern);
expectedmatch = Array(string, undefined, "Seattle", "WA", undefined, "Buckley", "WA");
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
