/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     switch.js
   Description:  'Tests the switch statement'

   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=323696

   Author:       Nick Lerissa
   Date:         March 19, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE   = 'statements: switch';
var BUGNUMBER="323696";

printBugNumber(BUGNUMBER);
writeHeaderToLog("Executing script: switch.js");
writeHeaderToLog( SECTION + " "+ TITLE);


var var1 = "match string";
var match1 = false;
var match2 = false;
var match3 = false;

switch (var1)
{
case "match string":
  match1 = true;
case "bad string 1":
  match2 = true;
  break;
case "bad string 2":
  match3 = true;
}

new TestCase ( 'switch statement',
	       true, match1);

new TestCase ( 'switch statement',
	       true, match2);

new TestCase ( 'switch statement',
	       false, match3);

var var2 = 3;

var match1 = false;
var match2 = false;
var match3 = false;
var match4 = false;
var match5 = false;

switch (var2)
{
case 1:
/*	        switch (var1)
  {
  case "foo":
  match1 = true;
  break;
  case 3:
  match2 = true;
  break;
  }*/
  match3 = true;
  break;
case 2:
  match4 = true;
  break;
case 3:
  match5 = true;
  break;
}
new TestCase ( 'switch statement',
	       false, match1);

new TestCase ( 'switch statement',
	       false, match2);

new TestCase ( 'switch statement',
	       false, match3);

new TestCase ( 'switch statement',
	       false, match4);

new TestCase ( 'switch statement',
	       true, match5);

test();
