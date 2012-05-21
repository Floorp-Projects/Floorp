/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    04 November 2003
 * SUMMARY: Testing regexps with various disjunction + character class patterns
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=224676
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 224676;
var summary = 'Regexps with various disjunction + character class patterns';
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


string = 'ZZZxZZZ';
status = inSection(1);
pattern = /[x]|x/;
actualmatch = string.match(pattern);
expectedmatch = Array('x');
addThis();

status = inSection(2);
pattern = /x|[x]/;
actualmatch = string.match(pattern);
expectedmatch = Array('x');
addThis();


string = 'ZZZxbZZZ';
status = inSection(3);
pattern = /a|[x]b/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();

status = inSection(4);
pattern = /[x]b|a/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();

status = inSection(5);
pattern = /([x]b|a)/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb', 'xb');
addThis();

status = inSection(6);
pattern = /([x]b|a)|a/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb', 'xb');
addThis();

status = inSection(7);
pattern = /^[x]b|a/;
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();


string = 'xb';
status = inSection(8);
pattern = /^[x]b|a/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();


string = 'ZZZxbZZZ';
status = inSection(9);
pattern = /([x]b)|a/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb', 'xb');
addThis();

status = inSection(10);
pattern = /()[x]b|a/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb', '');
addThis();

status = inSection(11);
pattern = /x[b]|a/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();

status = inSection(12);
pattern = /[x]{1}b|a/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();

status = inSection(13);
pattern = /[x]b|a|a/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();

status = inSection(14);
pattern = /[x]b|[a]/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();

status = inSection(15);
pattern = /[x]b|a+/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();

status = inSection(16);
pattern = /[x]b|a{1}/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();

status = inSection(17);
pattern = /[x]b|(a)/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb', undefined);
addThis();

status = inSection(18);
pattern = /[x]b|()a/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb', undefined);
addThis();

status = inSection(19);
pattern = /[x]b|^a/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();

status = inSection(20);
pattern = /a|[^b]b/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();

status = inSection(21);
pattern = /a|[^b]{1}b/;
actualmatch = string.match(pattern);
expectedmatch = Array('xb');
addThis();


string = 'hallo\";';
status = inSection(22);
pattern = /^((\\[^\x00-\x1f]|[^\x00-\x1f"\\])*)"/;
actualmatch = string.match(pattern);
expectedmatch = Array('hallo"', 'hallo', 'o');
addThis();

//----------------------------------------------------------------------------
test();
//----------------------------------------------------------------------------

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
