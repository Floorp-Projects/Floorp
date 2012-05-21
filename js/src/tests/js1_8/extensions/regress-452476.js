// |reftest| skip-if(Android) -- bug - nsIDOMWindow.crypto throws NS_ERROR_NOT_IMPLEMENTED on Android
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452476;
var summary = 'Do not assert with JIT: !cx->runningJittedCode';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);
 
for (var j = 0; j < 10; j++)
{
  for (var i = 0; i < j; ++i) 
    this["n" + i] = 1;

  this.__defineGetter__('w', (function(){}));

  [1 for each (g in this) for each (t in /x/g)];
}

jit(false);

reportCompare(expect, actual, summary);
