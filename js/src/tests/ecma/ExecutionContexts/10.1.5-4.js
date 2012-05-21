/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          10.1.5-4.js
   ECMA Section:       10.1.5 Global Object
   Description:
   There is a unique global object which is created before control enters
   any execution context. Initially the global object has the following
   properties:

   Built-in objects such as Math, String, Date, parseInt, etc. These have
   attributes { DontEnum }.

   Additional host defined properties. This may include a property whose
   value is the global object itself, for example window in HTML.

   As control enters execution contexts, and as ECMAScript code is executed,
   additional properties may be added to the global object and the initial
   properties may be changed.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "10.5.1-4";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Global Object");

new TestCase( "SECTION", "Anonymous Code check" );


var EVAL_STRING = 'if ( Object == null ) { gTestcases[0].reason += " Object == null" ; }' +
  'if ( Function == null ) { gTestcases[0].reason += " Function == null"; }' +
  'if ( String == null ) { gTestcases[0].reason += " String == null"; }'   +
  'if ( Array == null ) { gTestcases[0].reason += " Array == null"; }'     +
  'if ( Number == null ) { gTestcases[0].reason += " Function == null";}'  +
  'if ( Math == null ) { gTestcases[0].reason += " Math == null"; }'       +
  'if ( Boolean == null ) { gTestcases[0].reason += " Boolean == null"; }' +
  'if ( Date  == null ) { gTestcases[0].reason += " Date == null"; }'      +
  'if ( eval == null ) { gTestcases[0].reason += " eval == null"; }'       +
  'if ( parseInt == null ) { gTestcases[0].reason += " parseInt == null"; }' ;

var NEW_FUNCTION = new Function( EVAL_STRING );

if ( gTestcases[0].reason != "" ) {
  gTestcases[0].actual = "fail";
} else {
  gTestcases[0].actual = "pass";
}
gTestcases[0].expect = "pass";

test();
