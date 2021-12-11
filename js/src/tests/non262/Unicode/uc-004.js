/* -*- tab-width: 8; indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var BUGNUMBER = 23612;

test();

function test()
{
  printStatus ("Unicode Characters 1C-1F with regexps test.");
  printBugNumber (BUGNUMBER);
   
  var ary = ["\u001Cfoo", "\u001Dfoo", "\u001Efoo", "\u001Ffoo"];
   
  for (var i in ary)
  {      
    reportCompare (0, ary[Number(i)].search(/^\Sfoo$/),
		   "Unicode characters 1C-1F in regexps, ary[" +
		   i + "] did not match \\S test (it should not.)");
    reportCompare (-1, ary[Number(i)].search(/^\sfoo$/),
		   "Unicode characters 1C-1F in regexps, ary[" +
		   i + "] matched \\s test (it should not.)");
  }
}
