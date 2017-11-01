/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 419018;
var summary = 'UMR in JSENUMERATE_INIT';
var actual = 'No Crash';
var expect = 'No Crash';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  print('This test must be run under valgrind to check if an UMR occurs in slowarray_enumerate');

  try
  {
    function parse() {
      var a = []; // need array init
      a["b"] = 1; // need to set obj property
      return a;
    }
    // var c; // can't declare c
    // var d = {}; // can't add this (weird!)
    // var d = ""; // nor this
    var x = parse(""); // won't crash without string arg (weird!)
    // var d = ""; // nor here
    for (var o in x)
      c[o]; // need to look up o in undefined object
  }
  catch(ex)
  {
  }

  reportCompare(expect, actual, summary);
}

