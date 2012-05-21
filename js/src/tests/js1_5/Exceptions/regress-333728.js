/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 333728;
var summary = 'Throw ReferenceErrors for typeof(...undef)';
var actual = '';
var expect = 'ReferenceError';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{
  actual = typeof (0, undef);
}
catch(ex)
{
  actual = ex.name;
}
 
reportCompare(expect, actual, summary + ': typeof (0, undef)');

try
{
  actual = typeof (0 || undef);
}
catch(ex)
{
  actual = ex.name;
}
 
reportCompare(expect, actual, summary + ': typeof (0 || undef)');

try
{
  actual = typeof (1 && undef);
}
catch(ex)
{
  actual = ex.name;
}
 
reportCompare(expect, actual, summary + ': typeof (1 && undef)');

/*
  try
  {
  actual = typeof (0 ? 0 : undef);
  }
  catch(ex)
  {
  actual = ex.name;
  }
 
  reportCompare(expect, actual, summary + ': typeof (0 ? 0 : undef)');
*/

/*
  try
  {
  actual = typeof (1 ? undef : 0);
  }
  catch(ex)
  {
  actual = ex.name;
  }
 
  reportCompare(expect, actual, summary + ': typeof (1 ? undef : 0)');
*/

try
{
  actual = typeof (!this ? 0 : undef);
}
catch(ex)
{
  actual = ex.name;
}
 
reportCompare(expect, actual, summary + ': typeof (!this ? 0 : undef)');
