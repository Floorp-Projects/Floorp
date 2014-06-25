/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Robert Sayre
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 459293;
var summary = 'Allow redeclaration of JSON';
var actual = '';
var expect = '';

  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  try
  {
    eval('var JSON = "foo";');
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);
