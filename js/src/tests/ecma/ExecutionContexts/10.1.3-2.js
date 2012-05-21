/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          10.1.3-1.js
   ECMA Section:       10.1.3
   Description:

   Author:             mozilla@florian.loitsch.com
   Date:               27 July 2005
*/

var SECTION = "10.1.3-2";
var VERSION = "ECMA_1";
var TITLE   = "Variable Instantiation:  Function Declarations";
var BUGNUMBER="299639";
startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

function f(g)
{
  function g() {
    return "g";
  };
  return g;
}

new TestCase(
  SECTION,
  "typeof f(\"parameter\")",
  "function",
  typeof f("parameter") );

test();

