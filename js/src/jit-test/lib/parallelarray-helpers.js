/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Explanation of minItemsTestingThreshold:
//
// If the volume of input items in a test is small, then all of them
// may be processed during warmup alone, and the parallel-invocation
// will trivially succeed even if we are intentionally trying to
// detect a failure.
//
// The maximum number of items processed by sequential warmups for
// ParallelArrayBuild is:
//      maxSeqItems = maxBailouts * numSlices * CHUNK_SIZE
//
// For maxBailouts = 3, maxSeqItems == 3 * 8 * 32 == 768
// For maxBailouts = 5, maxSeqItems == 5 * 8 * 32 == 1280
//
// Our test code does not have access to the values of these constants
// (maxBailouts, numSlices, CHUNK_SIZE).  Therefore, the value of
// minItemsTestingThreshold should be kept in sync with some value
// greater than maxSeqItems as calculated above.
//
// This is still imperfect since it assumes numSlices <= 8, but
// numSlices is machine-dependent.
// (TODO: consider exposing numSlices via builtin/TestingFunctions.cpp)

var minItemsTestingThreshold = 1024;

function build(n, f) {
  var result = [];
  for (var i = 0; i < n; i++)
    result.push(f(i));
  return result;
}

function range(n, m) {
  // Returns an array with [n..m] (include on n, exclusive on m)

  var result = [];
  for (var i = n; i < m; i++)
    result.push(i);
  return result;
}

function seq_scan(array, f) {
  // Simple sequential version of scan() that operates over an array

  var result = [];
  result[0] = array[0];
  for (var i = 1; i < array.length; i++) {
    result[i] = f(result[i-1], array[i]);
  }
  return result;
}

function assertAlmostEq(v1, v2) {
  if (v1 === v2)
    return true;
  // + and other fp ops can vary somewhat when run in parallel!
  assertEq(typeof v1, "number");
  assertEq(typeof v2, "number");
  var diff = Math.abs(v1 - v2);
  var percent = diff / v1 * 100.0;
  print("v1 = " + v1);
  print("v2 = " + v2);
  print("% diff = " + percent);
  assertEq(percent < 1e-10, true); // off by an less than 1e-10%...good enough.
}

function assertStructuralEq(e1, e2) {
    if (e1 instanceof ParallelArray && e2 instanceof ParallelArray) {
      assertEqParallelArray(e1, e2);
    } else if (e1 instanceof Array && e2 instanceof ParallelArray) {
      assertEqParallelArrayArray(e2, e1);
    } else if (e1 instanceof ParallelArray && e2 instanceof Array) {
      assertEqParallelArrayArray(e1, e2);
    } else if (e1 instanceof Array && e2 instanceof Array) {
      assertEqArray(e1, e2);
    } else if (e1 instanceof Object && e2 instanceof Object) {
      assertEq(e1.__proto__, e2.__proto__);
      for (prop in e1) {
        if (e1.hasOwnProperty(prop)) {
          assertEq(e2.hasOwnProperty(prop), true);
          assertStructuralEq(e1[prop], e2[prop]);
        }
      }
    } else {
      assertEq(e1, e2);
    }
}

function assertEqParallelArrayArray(a, b) {
  assertEq(a.shape.length, 1);
  assertEq(a.length, b.length);
  for (var i = 0, l = a.length; i < l; i++) {
    try {
      assertStructuralEq(a.get(i), b[i]);
    } catch (e) {
      print("...in index ", i, " of ", l);
      throw e;
    }
  }
}

function assertEqArray(a, b) {
    assertEq(a.length, b.length);
    for (var i = 0, l = a.length; i < l; i++) {
      try {
        assertStructuralEq(a[i], b[i]);
      } catch (e) {
        print("...in index ", i, " of ", l);
        throw e;
      }
    }
}

function assertEqParallelArray(a, b) {
  assertEq(a instanceof ParallelArray, true);
  assertEq(b instanceof ParallelArray, true);

  var shape = a.shape;
  assertEqArray(shape, b.shape);

  function bump(indices) {
    var d = indices.length - 1;
    while (d >= 0) {
      if (++indices[d] < shape[d])
        break;
      indices[d] = 0;
      d--;
    }
    return d >= 0;
  }

  var iv = shape.map(function () { return 0; });
  do {
    try {
      var e1 = a.get.apply(a, iv);
      var e2 = b.get.apply(b, iv);
      assertStructuralEq(e1, e2);
    } catch (e) {
      print("...in indices ", iv, " of ", shape);
      throw e;
    }
  } while (bump(iv));
}

function assertParallelArrayModesEq(modes, acc, opFunction, cmpFunction) {
  if (!cmpFunction) { cmpFunction = assertStructuralEq; }
  modes.forEach(function (mode) {
    var result = opFunction({ mode: mode, expect: "success" });
    cmpFunction(acc, result);
  });
}

function assertParallelArrayModesCommute(modes, opFunction) {
    var acc = opFunction({ mode: modes[0], expect: "success" });
    assertParallelArrayModesEq(modes.slice(1), acc, opFunction);
}

function comparePerformance(opts) {
    var measurements = [];
    for (var i = 0; i < opts.length; i++) {
        var start = new Date();
        opts[i].func();
        var end = new Date();
        var diff = (end.getTime() - start.getTime());
        measurements.push(diff);
        print("Option " + opts[i].name + " took " + diff + "ms");
    }

    for (var i = 1; i < opts.length; i++) {
        var rel = (measurements[i] - measurements[0]) * 100 / measurements[0];
        print("Option " + opts[i].name + " relative to option " +
              opts[0].name + ": " + (rel|0) + "%");
    }
}

function compareAgainstArray(jsarray, opname, func, cmpFunction) {
  var expected = jsarray[opname].apply(jsarray, [func]);
  var parray = new ParallelArray(jsarray);

  // Unfortunately, it sometimes happens that running 'par' twice in a
  // row causes bailouts and other unfortunate things!

  assertParallelArrayModesEq(["seq", "par", "par"], expected, function(m) {
    print(m.mode + " " + m.expect);
    var result = parray[opname].apply(parray, [func, m]);
    // print(result.toString());
    return result;
  }, cmpFunction);
}

function testFilter(jsarray, func, cmpFunction) {
  compareAgainstArray(jsarray, "filter", func, cmpFunction);

  // var expected = jsarray.filter(func);
  // var filters = jsarray.map(func);
  // var parray = new ParallelArray(jsarray);
  //
  // // Unfortunately, it sometimes happens that running 'par' twice in a
  // // row causes bailouts and other unfortunate things!
  //
  // assertParallelArrayModesEq(["seq", "par", "par"], expected, function(m) {
  //   print(m.mode + " " + m.expect);
  //   return parray.filter(filters, m);
  // }, cmpFunction);
}

function testScan(jsarray, func, cmpFunction) {
  var expected = seq_scan(jsarray, func);
  var parray = new ParallelArray(jsarray);

  // Unfortunately, it sometimes happens that running 'par' twice in a
  // row causes bailouts and other unfortunate things!

  assertParallelArrayModesEq(["seq", "par", "par"], expected, function(m) {
    print(m.mode + " " + m.expect);
    var p = parray.scan(func, m);
    return p;
  }, cmpFunction);
}
