/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Function.prototype.toString should not quote XML literals";
var BUGNUMBER = 301692;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

actual = ConvertToString((function () { return <xml/>; }));
expect = 'function () { return <xml/>;}';
TEST(1, expect, actual);

actual = ConvertToString((function () { return <xml></xml>; }));
expect = 'function () { return <xml></xml>;}';
TEST(2, expect, actual);

actual = ConvertToString((function () { return <xml><morexml/></xml>; }));
expect = 'function () { return <xml><morexml/></xml>;}';
TEST(3, expect, actual);

actual = ConvertToString((function (k) { return <xml>{k}</xml>; }));
expect = 'function (k) { return <xml>{k}</xml>;}';
TEST(4, expect, actual);

actual = ConvertToString((function (k) { return <{k}/>; }));
expect = 'function (k) { return <{k}/>;}';
TEST(5, expect, actual);

actual = ConvertToString((function (k) { return <{k}>{k}</{k}>; }));
expect = 'function (k) { return <{k}>{k}</{k}>;}';
TEST(6, expect, actual);

actual = ConvertToString((function (k) { return <{k}
                         {k}={k} {"k"}={k + "world"}><{k + "k"}/></{k}>; }));
expect = 'function (k) ' +
         '{ return <{k} {k}={k} {"k"}={k + "world"}><{k + "k"}/></{k}>;}';
TEST(7, expect, actual);

END();

function ConvertToString(f)
{
  return f.toString().replace(/\n/g, '').replace(/[ ]+/g, ' ');
}
