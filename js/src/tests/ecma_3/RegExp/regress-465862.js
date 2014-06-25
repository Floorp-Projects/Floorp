/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465862;
var summary = 'Do case-insensitive matching correctly in JIT for non-ASCII-letters';

var i = 0;
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

// Note: we must call the RegExp constructor here instead of using
// literals. Otherwise, because the regexps are compiled at parse
// time, they will not be compiled to native code and we will not
// actually be testing jitted regexps.

jit(true);

status = inSection(1);
string = '@';
pattern = new RegExp('@', 'i');
actualmatch = string.match(pattern);
expectedmatch = Array(string);
addThis();

status = inSection(2);
string = '`';
pattern = new RegExp('`', 'i');
actualmatch = string.match(pattern);
expectedmatch = Array(string);
addThis();

status = inSection(3);
string = '@';
pattern = new RegExp('`', 'i');
actualmatch = string.match(pattern);
expectedmatch = null;
addThis();

status = inSection(4);
string = '`';
pattern = new RegExp('@', 'i');
print(string + ' ' + pattern);
actualmatch = string.match(pattern);
print('z ' + actualmatch);
print('`'.match(/@/i));
expectedmatch = null;
addThis();

jit(false);

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
