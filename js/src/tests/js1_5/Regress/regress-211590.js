/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 211590;
var summary = 'Math.random should be random';
var actual = '';
var expect = 'between 47.5% and 52.5%';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var r = Math.random;
var c = Math.pow( 2, 53 );

var n = 10000;
var odd1 = 0;
var odd2 = 0;

for ( var i = 0; i < n; ++i )
{
  var v= r() * c;
  if ( v & 1 )
    ++odd1;
  if ( v - c + c & 1 )
    ++odd2;
}

odd1 *= 100 / n;
odd2 *= 100 / n;

if (odd1 >= 47.5 && odd1 <= 52.5)
{
  actual = expect;
}
else
{
  actual = ' is ' + odd1.toFixed(3);
}

reportCompare(expect, actual, summary);

if (odd2 >= 47.5 && odd2 <= 52.5)
{
  actual = expect;
}
else
{
  actual = ' is ' + odd2.toFixed(3);
}

reportCompare(expect, actual, summary);
