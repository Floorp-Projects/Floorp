/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


test();

function test()
{
  var n0 = 1e23;
  var n1 = 5e22;
  var n2 = 1.6e24;

  printStatus ("Number formatting test.");
  printBugNumber ("11178");

  reportCompare ("1e+23", n0.toString(), "1e23 toString()");
  reportCompare ("5e+22", n1.toString(), "5e22 toString()");
  reportCompare ("1.6e+24", n2.toString(), "1.6e24 toString()");
   
}


