/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 340526;
var summary = 'Iterators: cross-referenced objects with close handler can ' +
  'delay close handler execution';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{
  var iter = Iterator({});
  iter.foo = "bar";
  for (var i in iter)
    ;
}
catch(ex)
{
}
 
reportCompare(expect, actual, summary);
