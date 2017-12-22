/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var BUGNUMBER = 24712;

test();

function test()
{   
  printBugNumber (BUGNUMBER);
   
  var re = /([\S]+([ \t]+[\S]+)*)[ \t]*=[ \t]*[\S]+/;
  var result = re.exec("Course_Creator = Test") + '';

  reportCompare('Course_Creator = Test,Course_Creator,', result, 'exec() returned null');
}

