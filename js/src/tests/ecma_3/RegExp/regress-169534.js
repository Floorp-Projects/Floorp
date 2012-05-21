/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    20 Sep 2002
 * SUMMARY: RegExp conformance test
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=169534
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 169534;
var summary = 'RegExp conformance test';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


status = inSection(1);
var re = /(\|)([\w\x81-\xff ]*)(\|)([\/a-z][\w:\/\.]*\.[a-z]{3,4})(\|)/ig;
var str = "To sign up click |here|https://www.xxxx.org/subscribe.htm|";
actual = str.replace(re, '<a href="$4">$2</a>');
expect = 'To sign up click <a href="https://www.xxxx.org/subscribe.htm">here</a>';
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
