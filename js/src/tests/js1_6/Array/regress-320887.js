// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Blake Kaplan
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 320887;
var summary = 'var x should not throw a ReferenceError';
var actual = 'No error';
var expect = 'No error';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{
  (function xxx() { ["var x"].map(eval); })()
    }
catch(ex)
{
  actual = ex + '';
}
 
reportCompare(expect, actual, summary);
