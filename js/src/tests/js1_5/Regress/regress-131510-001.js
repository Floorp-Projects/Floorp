/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    16 Mar 2002
 * SUMMARY: Shouldn't crash if define |var arguments| inside a function
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=131510
 *
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 131510;
var summary = "Shouldn't crash if define |var arguments| inside a function";
printBugNumber(BUGNUMBER);
printStatus(summary);


function f() {var arguments;}
f();


/*
 * Put same example in function scope instead of global scope
 */
function g() { function f() {var arguments;}; f();};
g();


/*
 * Put these examples in eval scope
 */
var s = 'function f() {var arguments;}; f();';
eval(s);

s = 'function g() { function f() {var arguments;}; f();}; g();';
eval(s);

reportCompare('No Crash', 'No Crash', '');
