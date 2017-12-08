/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          while-001
 *  ECMA Section:
 *  Description:        while statement
 *
 *  Verify that the while statement is not executed if the while expression is
 *  false
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "while-001";
var TITLE   = "while statement";

writeHeaderToLog( SECTION + " "+ TITLE);

DoWhile();
test();

function DoWhile() {
  result = "pass";

  while (false) {
    result = "fail";
    break;
  }

  new TestCase(
    "while statement: don't evaluate statement is expression is false",
    "pass",
    result );

}
