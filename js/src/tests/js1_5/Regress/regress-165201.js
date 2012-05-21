/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 165201;
var summary = '';
var actual = '';
var expect = '';


summary = 'RegExp.prototype.toSource should not affect RegExp.prototype.toString';

printBugNumber(BUGNUMBER);
printStatus (summary);

/*
 * Define function returning a regular expression literal
 * and override RegExp.prototype.toSource
 */

function f()
{
  return /abc/;
}

RegExp.prototype.toSource = function() { return 'Hi there'; };

expect = -1;
actual = f.toString().indexOf('Hi there');

reportCompare(expect, actual, summary);

/*
 * Define function returning an array literal
 * and override RegExp.prototype.toSource
 */
summary = 'Array.prototype.toSource should not affect Array.prototype.toString';
printBugNumber(BUGNUMBER);
printStatus (summary);

function g()
{
  return [1,2,3];
}

Array.prototype.toSource = function() { return 'Hi there'; }

  expect = -1;
actual = g.toString().indexOf('Hi there');

reportCompare(expect, actual, summary);


