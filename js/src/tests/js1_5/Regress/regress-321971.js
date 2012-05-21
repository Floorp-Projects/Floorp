/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 321971;
var summary = 'JSOP_FINDNAME replaces JSOP_BINDNAME';
var actual = 'no error';
var expect = 'no error';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var s='';
for (i=0; i < 1<<16; i++)
  s += 'x' + i + '=' + i + ';\n';

s += 'foo=' + i + ';\n';
eval(s);
foo;

reportCompare(expect, actual, summary);
