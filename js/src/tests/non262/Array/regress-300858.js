/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 300858;
var summary = 'Do not crash when sorting array with holes';
var actual = 'No Crash';
var expect = 'No Crash';

var arry     = [];
arry[6]  = 'six';
arry[8]  = 'eight';
arry[9]  = 'nine';
arry[13] = 'thirteen';
arry[14] = 'fourteen';
arry[21] = 'twentyone';
arry.sort();

reportCompare(expect, actual, summary);
