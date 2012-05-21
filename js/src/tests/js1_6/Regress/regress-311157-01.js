/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 311157;
var summary = 'Comment-hiding compromise left E4X parsing/scanning inconsistent';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{
  eval('var x = <hi> <!-- duh -->\n     there </hi>');
}
catch(e)
{
}

reportCompare(expect, actual, summary);

