/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 334158;
var summary = 'Parse error in control letter escapes (RegExp)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = true;
actual = /\ca/.test( "\x01" ); 
reportCompare(expect, actual, summary + ':/\ca/.test( "\x01" )');

expect = false
  actual = /\ca/.test( "\\ca" );
reportCompare(expect, actual, summary + ': /\ca/.test( "\\ca" )');

expect = false
  actual = /\c[a/]/.test( "\x1ba/]" );
reportCompare(expect, actual, summary + ': /\c[a/]/.test( "\x1ba/]" )');
