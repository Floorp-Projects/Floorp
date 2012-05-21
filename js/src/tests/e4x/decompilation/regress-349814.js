/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 349814;
var summary = 'decompilation of e4x literals';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var z = function ()
{
  a =
    <x>
      <y/>
    </x>;
};

expect = z + '';
actual = (eval("(" + z + ")")) + '';

compareSource(expect, actual, inSection(1) + summary);

END();
