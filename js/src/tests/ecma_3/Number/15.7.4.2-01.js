/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = "411889";
var summary = "num.toString(), num.toString(10), and num.toString(undefined)" +
              " should all be equivalent";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

try
{
  var noargs = 3.3.toString();
  var tenarg = 3.3.toString(10);
  var undefarg = 3.3.toString(undefined);

  if (noargs !== tenarg)
    throw "() !== (10): " + noargs + " !== " + tenarg;
  if (tenarg !== undefarg)
    throw "(10) !== (undefined): " + tenarg + " !== " + undefarg;
}
catch (e)
{
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);

expect = 1;
actual = 3.3.toString.length;
reportCompare(expect, actual, '3.3.toString.length should be 1');
