/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          dowhile-001
 *  ECMA Section:
 *  Description:        do...while statements
 *
 *
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "dowhile-002";
var TITLE   = "do...while with a labeled continue statement";

writeHeaderToLog( SECTION + " "+ TITLE);

LabeledContinue( 0, 1 );
LabeledContinue( 1, 1 );
LabeledContinue( -1, 1 );
LabeledContinue( 5, 5 );

test();

function LabeledContinue( limit, expect ) {
  i = 0;
woohoo:
  do {
    i++;
    continue woohoo;
  } while ( i < limit );

  new TestCase(
    "do while ( " + i +" < " + limit +" )",
    expect,
    i );
}
