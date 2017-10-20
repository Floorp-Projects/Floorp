// |reftest| skip-if(!xulRuntime.shell||Android) slow -- bug 504632
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 456826;
var summary = 'Do not assert with JIT during OOM';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  if (typeof gcparam != 'undefined')
  {
    gc();
    gc();
    gcparam("maxBytes", gcparam("gcBytes") + 4*1024);
    expectExitCode(5);
  }

  const numRows = 600;
  const numCols = 600;
  var scratch = {};
  var scratchZ = {};

  function complexMult(a, b) {
    var newr = a.r * b.r - a.i * b.i;
    var newi = a.r * b.i + a.i * b.r;
    scratch.r = newr;
    scratch.i = newi;
    return scratch;
  }

  function complexAdd(a, b) {
    scratch.r = a.r + b.r;
    scratch.i = a.i + b.i;
    return scratch;
  }

  function abs(a) {
    return Math.sqrt(a.r * a.r + a.i * a.i);
  }

  function computeEscapeSpeed(c) {
    scratchZ.r = scratchZ.i = 0;
    const scaler = 5;
    const threshold = (colors.length - 1) * scaler + 1;
    for (var i = 1; i < threshold; ++i) {
      scratchZ = complexAdd(c, complexMult(scratchZ, scratchZ));
      if (scratchZ.r * scratchZ.r + scratchZ.i * scratchZ.i > 4) {
        return Math.floor((i - 1) / scaler) + 1;
      }
    }
    return 0;
  }

  const colorStrings = [
    "black",
    "green",
    "blue",
    "red",
    "purple",
    "orange",
    "cyan",
    "yellow",
    "magenta",
    "brown",
    "pink",
    "chartreuse",
    "darkorange",
    "crimson",
    "gray",
    "deeppink",
    "firebrick",
    "lavender",
    "lawngreen",
    "lightsalmon",
    "lime",
    "goldenrod"
    ];

  var colors = [];

  function createMandelSet(realRange, imagRange) {
    var start = new Date();

    // Set up our colors
    for (var color of colorStrings) {
        var [r, g, b] = [0, 0, 0];
        colors.push([r, g, b, 0xff]);
      }

    var realStep = (realRange.max - realRange.min)/numCols;
    var imagStep = (imagRange.min - imagRange.max)/numRows;
    for (var i = 0, curReal = realRange.min;
         i < numCols;
         ++i, curReal += realStep) {
      for (var j = 0, curImag = imagRange.max;
           j < numRows;
           ++j, curImag += imagStep) {
        var c = { r: curReal, i: curImag }
        var n = computeEscapeSpeed(c);
      }
    }
    print(Date.now() - start);
  }

  var realRange = { min: -2.1, max: 2 };
  var imagRange = { min: -2, max: 2 };
  createMandelSet(realRange, imagRange);


  reportCompare(expect, actual, summary);
}
