// |reftest| skip-if(!xulRuntime.shell||xulRuntime.shell&&xulRuntime.XPCOMABI.match(/x86_64/)) slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 458679;
var summary = 'Do not assert: nbytes != 0';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function f()
{
  for (var i = 1; i < dps.length; ++i) {
    var a = "";
    var b = "";
    var c = "";
  }
}

function stringOfLength(n)
{
  if (n == 0) {
    return "";
  } else if (n == 1) {
    return "\"";
  } else {
    var r = n % 2;
    var d = (n - r) / 2;
    var y = stringOfLength(d);
    return y + y + stringOfLength(r);
  }    
}

try
{
  this.__defineGetter__('x', this.toSource);
  while (x.length < 12000000) { 
    let q = x;
    s = q + q; 
  }
  print(x.length);
}
catch(ex)
{
}
 
reportCompare(expect, actual, summary);
