// |reftest| skip-if(Android) -- bug - nsIDOMWindow.crypto throws NS_ERROR_NOT_IMPLEMENTED on Android
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 476427;
var summary = 'Do not assert: v != JSVAL_ERROR_COOKIE';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
function a1(){}
a2 = a1;
a3 = a1;
a4 = a1;
c1 = toString;
c2 = toSource;
function foo(code)
{
  (new Function(code))();
  delete toSource;
  delete toString;
  toSource = c2;
  toString = c1;
}
let z;
for (z = 1; z <= 16322; ++z) {
  this.__defineGetter__('functional', function x(){ yield; } );
  foo("this.__defineSetter__('', function(){});");
  foo("for each (y in this);");
}

reportCompare(expect, actual, summary);
