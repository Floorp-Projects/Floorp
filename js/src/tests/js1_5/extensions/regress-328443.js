/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 328443;
var summary = 'Uncatchable exception with |new (G.call) (F);| when F proto is null';
var actual = '';
var expect = 'Exception caught';

printBugNumber(BUGNUMBER);
printStatus (summary);

var F = (function(){});
F.__proto__ = null;

var G = (function(){});

var z;

z = "uncatchable exception!!!";
try {
  new (G.call) (F);

  actual = "No exception";
} catch (er) {
  actual = "Exception caught";
  printStatus("Exception was caught: " + er);
}
 
reportCompare(expect, actual, summary);
