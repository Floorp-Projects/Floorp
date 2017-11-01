/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    19 June 2003
 * SUMMARY: Testing regexp submatches with quantifiers
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=209919
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 209919;
var summary = 'Testing regexp submatches with quantifiers';
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
 * Waldemar: "ECMA-262 15.10.2.5, third algorithm, step 2.1 states that
 * once the minimum repeat count (which is 0 for *, 1 for +, etc.) has
 * been satisfied, an atom being repeated must not match the empty string."
 *
 * In this example, the minimum repeat count is 0, so the last thing the
 * capturing parens is permitted to contain is the 'a'. It may NOT go on
 * to capture the '' at the $ position of 'a', even though '' satifies
 * the condition b*
 *
 */
status = inSection(1);
string = 'a';
pattern = /(a|b*)*/;
actualmatch = string.match(pattern);
expectedmatch = Array(string, 'a');
addThis();


/*
 * In this example, the minimum repeat count is 5, so the capturing parens
 * captures the 'a', then goes on to capture the '' at the $ position of 'a'
 * 4 times before it has to stop. Therefore the last thing it contains is ''.
 */
status = inSection(2);
string = 'a';
pattern = /(a|b*){5,}/;
actualmatch = string.match(pattern);
expectedmatch = Array(string, '');
addThis();


/*
 * Reduction of the above examples to contain only the condition b*
 * inside the capturing parens. This can be even harder to grasp!
 *
 * The global match is the '' at the ^ position of 'a', but the parens
 * is NOT permitted to capture it since the minimum repeat count is 0!
 */
status = inSection(3);
string = 'a';
pattern = /(b*)*/;
actualmatch = string.match(pattern);
expectedmatch = Array('', undefined);
addThis();


/*
 * Here we have used the + quantifier (repeat count 1) outside the parens.
 * Therefore the parens must capture at least once before stopping, so it
 * does capture the '' this time -
 */
status = inSection(4);
string = 'a';
pattern = /(b*)+/;
actualmatch = string.match(pattern);
expectedmatch = Array('', '');
addThis();


/*
 * More complex examples -
 */
pattern = /^\-?(\d{1,}|\.{0,})*(\,\d{1,})?$/;

status = inSection(5);
string = '100.00';
actualmatch = string.match(pattern);
expectedmatch = Array(string, '00', undefined);
addThis();

status = inSection(6);
string = '100,00';
actualmatch = string.match(pattern);
expectedmatch = Array(string, '100', ',00');
addThis();

status = inSection(7);
string = '1.000,00';
actualmatch = string.match(pattern);
expectedmatch = Array(string, '000', ',00');
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
