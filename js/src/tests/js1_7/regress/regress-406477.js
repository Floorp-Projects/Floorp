/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 406477;
var summary = 'eval of function x() in a function with an argument "x" and "let x"';
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

  function test2(x, src)
  {
    var y = 1;
    {
      let x = 2;
      let y = 2;
      eval(src);
    }
    return [x, y];
  }

  var expect = actual = '';

  var [test_param_result, test_var_result] =
    test2(1, "function x() { }\nfunction y() { }\n");

  if (typeof test_param_result != "function")
    actual += "Unexpected test_param_result value: "+uneval(test_param_result)+"\n";

  if (typeof test_var_result != "function")
    actual += "Unexpected test_var_result value: "+uneval(test_var_result)+"\n";

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
