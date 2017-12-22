// |reftest| skip -- slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 330684;
var summary = 'Do not hang on RegExp';
var actual = 'Do not hang on RegExp';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var re = /^(?:(?:%[0-9A-Fa-f]{2})*[!\$&'\*-;=\?-Z_a-z]*)+$/;
var url = "http://tw.yimg.com/a/tw/wenchuan/cam_240x400_381615_030806_2.swf?clickTAG=javascript:VRECopenWindow(1)";

printStatus(re.test(url));

reportCompare(expect, actual, summary);
