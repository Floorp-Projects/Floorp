/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 366123;
var summary = 'Compiling long XML filtering predicate';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

function exploit() {
  var code = "foo = <x/>.(", obj = {};
  for(var i = 0; i < 0x10000; i++) {
    code += "0, ";
  }
  code += "0);";
  Function(code);
}
try
{
    exploit();
}
catch(ex)
{
}

TEST(1, expect, actual);

END();
