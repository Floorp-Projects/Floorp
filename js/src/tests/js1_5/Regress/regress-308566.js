/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 308566;
var summary = 'Do not treat octal sequence as regexp backrefs in strict mode';
var actual = 'No error';
var expect = 'No error';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
options('strict');
options('werror');

try
{
  var c = eval("/\260/");
}
catch(e)
{
  actual = e + '';
}

reportCompare(expect, actual, summary);
