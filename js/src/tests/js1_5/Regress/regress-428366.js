/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Blake Kaplan
 */

var gTestfile = 'regress-428366.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 428366;
var summary = 'Do not assert deleting eval 16 times';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

this.__proto__.x = eval;
for (i = 0; i < 16; ++i) delete eval;
(function w() { x = 1; })();
 
reportCompare(expect, actual, summary);
