/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.8.2.js
   ECMA Section:       7.8.2 Examples of Automatic Semicolon Insertion
   Description:        compare some specific examples of the automatic
   insertion rules in the EMCA specification.
   Author:             christine@netscape.com
   Date:               15 september 1997
*/

var SECTION="7.8.2";
writeHeaderToLog(SECTION+" "+"Examples of Semicolon Insertion");


//    new TestCase( "{ 1 \n 2 } 3",      3,         eval("{ 1 \n 2 } 3") );

DESCRIPTION = "{ 1 2 } 3";

new TestCase( "{ 1 2 } 3",         "error",   eval("{1 2 } 3")     );

test();
