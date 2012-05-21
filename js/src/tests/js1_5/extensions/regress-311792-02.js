/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 311792;
var summary = 'Root Array.prototype methods';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var subverted = 0;

function index_getter()
{
  delete a[0];
  gc();
  for (var i = 0; i != 1 << 14; ++i) {
    var tmp = new String("test");
    tmp = null;
  }
  return 1;
}

function index_setter(value)
{
  subverted = value;
}

var a = [ Math.sqrt(2), 0 ];
a.__defineGetter__(1, index_getter);
a.__defineSetter__(1, index_setter);

a.reverse();
printStatus(subverted)

  reportCompare(expect, actual, summary);
