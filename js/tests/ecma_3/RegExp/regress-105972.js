/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS  IS"
* basis, WITHOUT WARRANTY OF ANY KIND, either expressed
* or implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.
* All Rights Reserved.
*
* Contributor(s): mozilla@pdavis.cx, pschwartau@netscape.com
* Date: 22 October 2001
*
* SUMMARY: Regression test for Bugzilla bug 105972:
* "/^.*?$/ will not match anything"
*
* See http://bugzilla.mozilla.org/show_bug.cgi?id=105972
*/
//-----------------------------------------------------------------------------
var i = 0;
var bug = 105972;
var summary = 'Regression test for Bugzilla bug 105972';
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


/*
 * The bug: this match was coming up null in Rhino and SpiderMonkey...
 */
status = inSection(1);
pattern = /^.*?$/;
string = 'Hello World';
actualmatch = string.match(pattern);
expectedmatch = Array(string);
addThis();


/*
 * Leave off the '$' condition - here we expect the empty string
 */
status = inSection(2);
pattern = /^.*?/;
string = 'Hello World';
actualmatch = string.match(pattern);
expectedmatch = [cnEmptyString];
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
  printBugNumber (bug);
  printStatus (summary);
  testRegExp(statusmessages, patterns, strings, actualmatches, expectedmatches);
  exitFunc ('test');
}
