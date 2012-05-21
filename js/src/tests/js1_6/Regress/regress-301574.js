/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 301574;
var summary = 'E4X should be enabled even when e4x=1 not specified';
var actual = 'No error';
var expect = 'No error';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{
  var xml = XML('<xml/>');
}
catch(e)
{
  actual = 'error: ' + e;
} 

reportCompare(expect, actual, summary + ': XML()');

try
{
  var xml = XMLList('<p>text</p>');
}
catch(e)
{
  actual = 'error: ' + e;
} 

reportCompare(expect, actual, summary + ': XMLList()');
