/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 317533;
var summary = 'improve function does not always return a value warnings';
var actual = '';
var expect = 'TypeError: anonymous function does not always return a value';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var code;

if (!options().match(/strict/))
{
  options('strict');
}
if (!options().match(/werror/))
{
  options('werror');
}

try
{
  actual = '';
  code = "(function(x){ if(x) return x; })";
  print(code);
  eval(code);
}
catch(ex)
{
  actual = ex + '';
}

reportCompare(expect, actual, summary);

try
{
  actual = '';
  code = "(function(x){ if(x) return x; ;})";
  print(code);
  eval(code);
}
catch(ex)
{
  actual = ex + '';
}

reportCompare(expect, actual, summary);
