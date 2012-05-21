/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("Testing replacing an element with a list that contains a text node");
printBugNumber(263936);

var x =
<x>
  <a>one</a>
  <b>three</b>
</x>;

// insert a text node "two" between elements <a> and <b>
x.a += XML("two");

var expected =
<x>
  <a>one</a>
  two
  <b>three</b>
</x>;

TEST(1, expected, x);

END();
