// |reftest| skip-if(Android) -- bug - nsIDOMWindow.crypto throws NS_ERROR_NOT_IMPLEMENTED on Android
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 475144;
var summary = 'TM: Do not assert: !JS_ON_TRACE(cx)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

function a() {}
function b() {}
function c() {}
eval("this.__defineGetter__(\"\", function(){ return new Function } )");
[[].some for each (x in this) for each (y in /x/g)];

jit(false);

reportCompare(expect, actual, summary);
