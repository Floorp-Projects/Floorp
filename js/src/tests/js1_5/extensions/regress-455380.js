/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Robert Sayre
 */

var gTestfile = 'regress-455380.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 455380;
var summary = 'Do not assert with JIT: !lhs->isQuad() && !rhs->isQuad()';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

const IS_TOKEN_ARRAY =
  [0, 0, 0, 0, 0, 0, 0, 0, //   0
   0, 0, 0, 0, 0, 0, 0, 0, //   8
   0, 0, 0, 0, 0, 0, 0, 0, //  16
   0, 0, 0, 0, 0, 0, 0, 0, //  24

   0, 1, 0, 1, 1, 1, 1, 1, //  32
   0, 0, 1, 1, 0, 1, 1, 0, //  40
   1, 1, 1, 1, 1, 1, 1, 1, //  48
   1, 1, 0, 0, 0, 0, 0, 0, //  56

   0, 1, 1, 1, 1, 1, 1, 1, //  64
   1, 1, 1, 1, 1, 1, 1, 1, //  72
   1, 1, 1, 1, 1, 1, 1, 1, //  80
   1, 1, 1, 0, 0, 0, 1, 1, //  88

   1, 1, 1, 1, 1, 1, 1, 1, //  96
   1, 1, 1, 1, 1, 1, 1, 1, // 104
   1, 1, 1, 1, 1, 1, 1, 1, // 112
   1, 1, 1, 0, 1, 0, 1];   // 120

const headerUtils = {
normalizeFieldName: function(fieldName)
{
  if (fieldName == "")
    throw "error: empty string";

  for (var i = 0, sz = fieldName.length; i < sz; i++)
  {
    if (!IS_TOKEN_ARRAY[fieldName.charCodeAt(i)])
    {
      throw (fieldName + " is not a valid header field name!");
    }
  }

  return fieldName.toLowerCase();
}
};

headerUtils.normalizeFieldName("Host");

jit(false);

reportCompare(expect, actual, summary);
