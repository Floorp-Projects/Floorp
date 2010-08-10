/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Robert Sayre
 */


var summary = 'decompilation of XMLPI object initializer';
var BUGNUMBER = 351988;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var f;
f = function() { var y = <?foo bar?>; }
actual = f + '';
expect = 'function () {\n    var y = <?foo bar?>;\n}';

compareSource(expect, actual, inSection(1) + summary);

END();
