/* -*- tab-width: 8; indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var BUGNUMBER = 23613;

test();

function test()
{
  printStatus ("Unicode non-breaking space character test.");
  printBugNumber (BUGNUMBER);

  reportCompare ("no error", eval("'no'\u00A0+ ' error'"),
		 "Unicode non-breaking space character test.");

  var str = "\u00A0foo";
  reportCompare (0, str.search(/^\sfoo$/),
		 "Unicode non-breaking space character regexp test.");
}
