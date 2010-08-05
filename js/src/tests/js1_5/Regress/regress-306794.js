/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Blake Kaplan
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 306794;
var summary = 'Do not assert: parsing foo getter';
var actual = 'No Assertion';
var expect = 'No Assertion';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
try
{
  eval('getter\n');
}
catch(e)
{
}

reportCompare(expect, actual, summary);
