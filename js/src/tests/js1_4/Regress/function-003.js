/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          function-003.js
 *  Description:
 *
 *  http://scopus.mcom.com/bugsplat/show_bug.cgi?id=104766
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "toString-001.js";
var VERSION = "JS1_4";
var TITLE   = "Regression test case for 104766";
var BUGNUMBER="310514";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(
  SECTION,
  "StripSpaces(Array.prototype.concat.toString()).substring(0,17)",
  "functionconcat(){",
  StripSpaces(Array.prototype.concat.toString()).substring(0,17));

test();

function StripSpaces( s ) {
  for ( var currentChar = 0, strippedString="";
        currentChar < s.length; currentChar++ )
  {
    if (!IsWhiteSpace(s.charAt(currentChar))) {
      strippedString += s.charAt(currentChar);
    }
  }
  return strippedString;
}

function IsWhiteSpace( string ) {
  var cc = string.charCodeAt(0);
  switch (cc) {
  case (0x0009):
  case (0x000B):
  case (0x000C):
  case (0x0020):
  case (0x000A):
  case (0x000D):
  case ( 59 ): // let's strip out semicolons, too
    return true;
    break;
  default:
    return false;
  }
}

