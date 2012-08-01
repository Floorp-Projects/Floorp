// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 621464;
var summary = '<x>a</x>.replace() == <x>a</x>';

printBugNumber(BUGNUMBER);
START(summary);

var expected = <x>a</x>;
var actual = <x>a</x>.replace();

TEST(0, expected, actual);

END();
