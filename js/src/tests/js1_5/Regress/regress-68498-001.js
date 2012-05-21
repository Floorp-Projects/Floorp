/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 15 Feb 2001
 *
 * SUMMARY: var self = global JS object, outside any eval, is DontDelete
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=68498
 * See http://bugzilla.mozilla.org/showattachment.cgi?attach_id=25251
 *
 * Brendan:
 *
 * "Demonstrate that variable statement outside any eval creates a
 *   DontDelete property of the global object"
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 68498;
var summary ='Testing that variable statement outside any eval creates'  +
  ' a DontDelete property of the global object';


// To be pedantic, use a variable named 'self' to capture the global object -
// conflicts with window.self in browsers
var _self = this;
var actual = (delete _self);
var expect =false;


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary); 
  reportCompare(expect, actual, summary);
  exitFunc ('test');
}
