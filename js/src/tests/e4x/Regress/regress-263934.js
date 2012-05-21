/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("Testing that replacing a list item with a new list that contains that item works");
printBugNumber(263934);

var x =
<x>
  <b>two</b>
  <b>three</b>
</x>;

// insert <a> element in from of first <b> element
x.b[0] = <a>one</a> + x.b[0];

var expected =
<x>
  <a>one</a>
  <b>two</b>
  <b>three</b>
</x>;


TEST(1, expected, x);

END();
