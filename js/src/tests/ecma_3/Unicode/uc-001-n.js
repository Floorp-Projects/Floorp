/* -*- tab-width: 8; indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


test();

function test()
{
  enterFunc ("test");

  printStatus ("Unicode Characters 1C-1F negative test.");
  printBugNumber (23612);
   
  reportCompare ("error", eval ("'no'\u001C+' error'"),
		 "Unicode whitespace test (1C.)");
  reportCompare ("error", eval ("'no'\u001D+' error'"),
		 "Unicode whitespace test (1D.)");
  reportCompare ("error", eval ("'no'\u001E+' error'"),
		 "Unicode whitespace test (1E.)");
  reportCompare ("error", eval ("'no'\u001F+' error'"),
		 "Unicode whitespace test (1F.)");

  exitFunc ("test");
}
