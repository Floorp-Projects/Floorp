// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 454142;
var summary = 'Do not crash with gczeal, setter, watch';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
this.watch("x", function(){});
delete x;
if (typeof gczeal == 'function')
{
  gczeal(2);
}

this.__defineSetter__("x", function(){});

if (typeof gczeal == 'function')
{
  gczeal(0);
}

reportCompare(expect, actual, summary);
