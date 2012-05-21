/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 329383;
var summary = 'Math copysign issues';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var inputs = [
  -Infinity,
  -10.01,
  -9.01,
  -8.01,
  -7.01,
  -6.01,
  -5.01,
  -4.01,
  -Math.PI,
  -3.01,
  -2.01,
  -1.01,
  -0.5,
  -0.49,
  -0.01,
  -0,
  0,
  +0,
  0.01,
  0.49,
  0.50,
  0,
  1.01,
  2.01,
  3.01,
  Math.PI,
  4.01,
  5.01,
  6.01,
  7.01,
  8.01,
  9.01,
  10.01,
  Infinity
  ];

var iinput;

for (iinput = 0; iinput < inputs.length; iinput++)
{
  var input = inputs[iinput];
  reportCompare(Math.round(input),
                emulateRound(input),
                summary + ': Math.round(' + input + ')');
}

reportCompare(isNaN(Math.round(NaN)),
              isNaN(emulateRound(NaN)),
              summary + ': Math.round(' + input + ')');

function emulateRound(x)
{
  if (!isFinite(x) || x === 0) return x
    if (-0.5 <= x && x < 0) return -0
      return Math.floor(x + 0.5)
      }

var z;

z = Math.min(-0, 0);

reportCompare(-Math.PI, Math.atan2(z, z), summary + ': Math.atan2(-0, -0)');
reportCompare(-Infinity, 1/z, summary + ': 1/-0');

z = Math.max(-0, 0);

reportCompare(0, Math.atan2(z, z), summary + ': Math.atan2(0, 0)');
reportCompare(Infinity, 1/z, summary + ': 1/0');
