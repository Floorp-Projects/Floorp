/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    12 December 2003
 * SUMMARY: Testing regexps with unescaped braces
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=228087
 *
 * Note: unbalanced, unescaped braces are not permitted by ECMA-262 Ed.3,
 * but we decided to follow Perl and IE and allow this for compatibility.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=188206 and its testcase.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=223273 and its testcase.
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 228087;
var summary = 'Testing regexps with unescaped braces';
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
var e;


string = 'foo {1} foo {2} foo';

// try an example with the braces escaped
status = inSection(1);
try
{
  pattern = new RegExp('\{1.*\}', 'g');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('{1} foo {2}');
addThis();

// just like Section 1, without the braces being escaped
status = inSection(2);
try
{
  pattern = new RegExp('{1.*}', 'g');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('{1} foo {2}');
addThis();

// try an example with the braces escaped
status = inSection(3);
try
{
  pattern = new RegExp('\{1[.!\}]*\}', 'g');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('{1}');
addThis();

// just like Section 3, without the braces being escaped
status = inSection(4);
try
{
  pattern = new RegExp('{1[.!}]*}', 'g');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('{1}');
addThis();


string = 'abccccc{3 }c{ 3}c{3, }c{3 ,}c{3 ,4}c{3, 4}c{3,4 }de';

// use braces in a normal quantifier construct
status = inSection(5);
try
{
  pattern = new RegExp('c{3}');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('ccc');
addThis();

// now disrupt the quantifer - the braces should now be interpreted literally
status = inSection(6);
try
{
  pattern = new RegExp('c{3 }');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('c{3 }');
addThis();

status = inSection(7);
try
{
  pattern = new RegExp('c{3.}');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('c{3 }');
addThis();

status = inSection(8);
try
{
  // need to escape the \ in \s since
  // this has been converted to a constructor call
  // instead of a literal regexp
  pattern = new RegExp('c{3\\s}');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('c{3 }');
addThis();

status = inSection(9);
try
{
  pattern = new RegExp('c{3[ ]}');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('c{3 }');
addThis();

status = inSection(10);
try
{
  pattern = new RegExp('c{ 3}');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('c{ 3}');
addThis();

// using braces in a normal quantifier construct again
status = inSection(11);
try
{
  pattern = new RegExp('c{3,}');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('ccccc');
addThis();

// now disrupt it - the braces should now be interpreted literally
status = inSection(12);
try
{
  pattern = new RegExp('c{3, }');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('c{3, }');
addThis();

status = inSection(13);
try
{
  pattern = new RegExp('c{3 ,}');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('c{3 ,}');
addThis();

// using braces in a normal quantifier construct again
status = inSection(14);
try
{
  pattern = new RegExp('c{3,4}');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('cccc');
addThis();

// now disrupt it - the braces should now be interpreted literally
status = inSection(15);
try
{
  pattern = new RegExp('c{3 ,4}');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('c{3 ,4}');
addThis();

status = inSection(16);
try
{
  pattern = new RegExp('c{3, 4}');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('c{3, 4}');
addThis();

status = inSection(17);
try
{
  pattern = new RegExp('c{3,4 }');
  actualmatch = string.match(pattern);
}
catch (e)
{
  pattern = 'error';
  actualmatch = '';
}
expectedmatch = Array('c{3,4 }');
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
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  testRegExp(statusmessages, patterns, strings, actualmatches, expectedmatches);
}
