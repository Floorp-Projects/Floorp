/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    09 July 2002
 * SUMMARY: RegExp conformance test
 *
 *   These testcases are derived from the examples in the ECMA-262 Ed.3 spec
 *   scattered through section 15.10.2.
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = '(none)';
var summary = 'RegExp conformance test';
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
pattern = /a|ab/;
string = 'abc';
actualmatch = string.match(pattern);
expectedmatch = Array('a');
addThis();

status = inSection(2);
pattern = /((a)|(ab))((c)|(bc))/;
string = 'abc';
actualmatch = string.match(pattern);
expectedmatch = Array('abc', 'a', 'a', undefined, 'bc', undefined, 'bc');
addThis();

status = inSection(3);
pattern = /a[a-z]{2,4}/;
string = 'abcdefghi';
actualmatch = string.match(pattern);
expectedmatch = Array('abcde');
addThis();

status = inSection(4);
pattern = /a[a-z]{2,4}?/;
string = 'abcdefghi';
actualmatch = string.match(pattern);
expectedmatch = Array('abc');
addThis();

status = inSection(5);
pattern = /(aa|aabaac|ba|b|c)*/;
string = 'aabaac';
actualmatch = string.match(pattern);
expectedmatch = Array('aaba', 'ba');
addThis();

status = inSection(6);
pattern = /^(a+)\1*,\1+$/;
string = 'aaaaaaaaaa,aaaaaaaaaaaaaaa';
actualmatch = string.match(pattern);
expectedmatch = Array('aaaaaaaaaa,aaaaaaaaaaaaaaa', 'aaaaa');
addThis();

status = inSection(7);
pattern = /(z)((a+)?(b+)?(c))*/;
string = 'zaacbbbcac';
actualmatch = string.match(pattern);
expectedmatch = Array('zaacbbbcac', 'z', 'ac', 'a', undefined, 'c');
addThis();

status = inSection(8);
pattern = /(a*)*/;
string = 'b';
actualmatch = string.match(pattern);
expectedmatch = Array('', undefined);
addThis();

status = inSection(9);
pattern = /(a*)b\1+/;
string = 'baaaac';
actualmatch = string.match(pattern);
expectedmatch = Array('b', '');
addThis();

status = inSection(10);
pattern = /(?=(a+))/;
string = 'baaabac';
actualmatch = string.match(pattern);
expectedmatch = Array('', 'aaa');
addThis();

status = inSection(11);
pattern = /(?=(a+))a*b\1/;
string = 'baaabac';
actualmatch = string.match(pattern);
expectedmatch = Array('aba', 'a');
addThis();

status = inSection(12);
pattern = /(.*?)a(?!(a+)b\2c)\2(.*)/;
string = 'baaabaac';
actualmatch = string.match(pattern);
expectedmatch = Array('baaabaac', 'ba', undefined, 'abaac');
addThis();

status = inSection(13);
pattern = /(?=(a+))/;
string = 'baaabac';
actualmatch = string.match(pattern);
expectedmatch = Array('', 'aaa');
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
