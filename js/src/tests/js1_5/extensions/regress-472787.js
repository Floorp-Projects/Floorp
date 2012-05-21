// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 472787;
var summary = 'Do not hang with gczeal, watch and concat';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

this.__defineSetter__("x", Math.sin);
this.watch("x", '' .concat);

if (typeof gczeal == 'function')
{
  gczeal(2);
}

x = 1;

if (typeof gczeal == 'function')
{
  gczeal(0);
}

reportCompare(expect, actual, summary);
