/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


test();

function test()
{
  enterFunc ("test");

  printStatus ("Unicode non-breaking space character test.");
  printBugNumber (23613);

  reportCompare ("no error", eval("'no'\u00A0+ ' error'"),
		 "Unicode non-breaking space character test.");

  var str = "\u00A0foo";
  reportCompare (0, str.search(/^\sfoo$/),
		 "Unicode non-breaking space character regexp test.");

  exitFunc ("test");
}
