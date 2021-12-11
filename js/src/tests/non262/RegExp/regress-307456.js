// |reftest| slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 307456;
var summary = 'Do not Freeze with RegExp';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

var data='<!---<->---->\n\n<><>--!!!<><><><><><>\n!!<>\n\n<>\n<><><><>!\n\n\n\n--\n--\n--\n\n--\n--\n\n\n-------\n--\n--\n\n\n--\n\n\n\n----\n\n\n\n--\n\n\n-\n\n\n-\n\n-\n\n-\n\n-\n-\n\n----\n\n-\n\n\n\n\n-\n\n\n\n\n\n\n\n\n-----\n\n\n-\n------\n-------\n\n----\n\n\n\n!\n\n\n\n\n\n\n\n!!!\n\n\n--------\n\n\n\n-\n\n\n-\n--\n\n----\n\n\n\n\n\n-\n\n\n----\n\n\n\n\n\n--------\n!\n\n\n\n\n-\n---\n--\n\n----\n\n-\n\n-\n\n-\n\n\n\n-----\n\n\n\n-\n\n\n-\n\n\n--\n-\n\n\n-\n\n----\n\n---\n\n---\n\n----\n\n\n\n---\n\n-++\n\n-------<>\n\n-!\n\n--\n\n----!-\n\n\n\n';

printStatus(data);
data=data.replace(RegExp('<!--(\\n[^\\n]|[^-]|-[^-]|--[^>])*-->', 'g'), '');
printStatus(data);

reportCompare(expect, actual, summary);
