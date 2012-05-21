/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 462989;
var summary = 'Do not assert: need a way to EOT now, since this is trace end';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
jit(true);

function a()
{
  "".split(";");
  this.v = true;
}

function b()
{    
  var z = { t: function() { for (var i = 0; i < 5; i++) { a(); } } };
  z.t();
}

b();

jit(false);

reportCompare(expect, actual, summary);
