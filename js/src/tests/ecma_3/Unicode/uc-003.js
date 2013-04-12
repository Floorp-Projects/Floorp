/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


test();

function test()
{
  enterFunc ("test");

  var \u0041 = 5;
  var A\u03B2 = 15;

  printStatus ("Escapes in identifiers test.");
  printBugNumber (23608);
  printBugNumber (23607);

  reportCompare (5, eval("\u0041"),
		 "Escaped ASCII Identifier test.");
  reportCompare (6, eval("++\u0041"),
		 "Escaped ASCII Identifier test");
  reportCompare (15, eval("A\u03B2"),
		 "Escaped non-ASCII Identifier test");
  reportCompare (16, eval("++A\u03B2"),
		 "Escaped non-ASCII Identifier test");
   
  exitFunc ("test");
}
