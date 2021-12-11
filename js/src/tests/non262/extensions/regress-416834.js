/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 416834;
var summary = 'Do not assert: !entry || entry->kpc == ((PCVCAP_TAG(entry->vcap) > 1) ? (jsbytecode *) JSID_TO_ATOM(id) : cx->fp->pc)';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
this.__proto__.x = eval;
for (i = 0; i < 16; ++i) 
  delete eval;
(function w() { x = 1; })();

reportCompare(expect, actual, summary);
