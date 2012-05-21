/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 328664;
var summary = 'Correct error message for funccall(undefined, undefined.prop)';
var actual = '';
var expect = /TypeError: value.parameters (has no properties|is undefined)/;

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var value = {};

function funccall(a,b)
{
}

try
{
  funccall(value[1], value.parameters[1]);
}
catch(ex)
{
  printStatus(ex);
  actual = ex + '';
}

reportMatch(expect, actual, summary);
