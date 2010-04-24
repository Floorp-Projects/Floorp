/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *      Rasmus Jensen <rje(a)dbc.dk>
 */

gTestfile = 'template.js';

var summary = 'Uneval+eval of XML containing string with {';
var BUGNUMBER = 463360;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

actual = eval(uneval(XML("<a>{</a>")));
expect = XML("<a>{</a>");

compareSource(expect, actual, inSection(1) + summary);

END();
