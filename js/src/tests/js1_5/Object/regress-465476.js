/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465476;
var summary = '"-0" and "0" are distinct properties.';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var x = { "0": 3, "-0": 7 };

  expect = actual = 'No Exception';

  try
  {
    if (!("0" in x))
      throw "0 not in x";
    if (!("-0" in x))
      throw "-0 not in x";
    delete x[0];
    if ("0" in x)
      throw "0 in x after delete";
    if (!("-0" in x))
      throw "-0 removed from x after unassociated delete";
    delete x["-0"];
    if ("-0" in x)
      throw "-0 in x after delete";
    x[0] = 3;
    if (!("0" in x))
      throw "0 not in x after insertion of 0 property";
    if ("-0" in x)
      throw "-0 in x after insertion of 0 property";
    x["-0"] = 7;
    if (!("-0" in x))
      throw "-0 not in x after reinsertion";

    var props = [];
    for (var i in x)
      props.push(i);
    if (props.length !== 2)
      throw "not all props found!";
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
