/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 474529;
var summary = 'Do not assert: _buf->_nextPage';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function main() {

    function timeit(N, buildArrayString) {
      return Function("N",
                      "var d1 = +new Date;" +
                      "while (N--) var x = " + buildArrayString +
                      "; return +new Date - d1")(N);
    }

    function reportResults(size, N, literalMs, newArrayMs, arrayMs) {
      print(Array.prototype.join.call(arguments, "\t"));
    }

    var repetitions = [ 9000, 7000, 4000, 2000, 2000, 2000, 800, 800, 800, 300, 100, 100 ]
      for (var zeros = "0, ", i = 0; i < repetitions.length; ++i) {
        var N = repetitions[i];
        reportResults((1 << i) + 1, N,
                      timeit(N, "[" + zeros + " 0 ]"),
                      timeit(N, "new Array(" + zeros + " 0)"),
                      timeit(N, "Array(" + zeros + " 0)"));
        zeros += zeros;
      }
  }

  gc();
  print("Size\t\Rep.\t\Literal\tnew Arr\tArray()");
  print("====\t=====\t=======\t=======\t=======");
  main();

  reportCompare(expect, actual, summary);
}
