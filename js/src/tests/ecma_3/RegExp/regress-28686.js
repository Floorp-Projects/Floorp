/* -*- tab-width: 8; indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var BUGNUMBER = 28686;

test();

function test()
{   
  printBugNumber (BUGNUMBER);
   
  var str = 'foo "bar" baz';
  reportCompare ('foo \\"bar\\" baz', str.replace(/([\'\"])/g, "\\$1"),
		 "str.replace failed.");
}
