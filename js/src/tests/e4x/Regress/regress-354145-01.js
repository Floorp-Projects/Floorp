/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 354145;
var summary = 'Immutable XML';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

var xml = <></>
var N = 10;
for (var i = 0; i != N; ++i) {
  xml[i] = <{"a"+i}/>;
}

function prepare()
{
  for (var i = N - 1; i >= 0; --i)
    delete xml[i];
  gc();
  return "test";
}

xml[N - 1] = { toString: prepare };

TEST(1, expect, actual);

END();
