/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 254296;
var summary = 'javascript regular expression negative lookahead';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = [3].toString();
actual = /^\d(?!\.\d)/.exec('3.A');
if (actual)
{
  actual = actual.toString();
}
 
reportCompare(expect, actual, summary + ' ' + inSection(1));

expect = 'AB';
actual = /(?!AB+D)AB/.exec("AB") + '';
reportCompare(expect, actual, summary + ' ' + inSection(2));
