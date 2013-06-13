DEFAULT_WARMUP = 10
DEFAULT_MEASURE = 3
MODE = MODE || "compare" // MODE is often set on the command-line by run.sh

/**
 * label: for the printouts
 * w: warmup runs
 * m: measurement runs
 * seq: closure to compute sequentially
 * par: closure to compute in parallel
 */
function benchmark(label, w, m, seq, par) {
  var SEQ = 1
  var PAR = 2
  var bits = 0
  if (MODE === "seq") { bits = SEQ; }
  else if (MODE === "par") { bits = PAR; }
  else {
    if (MODE !== "compare") {
      print("Invalid MODE, expected seq|par|compare: ", MODE);
    }
    bits = SEQ|PAR;
  }

  if (mode(SEQ)) {
    print("Warming up sequential runs");
    warmup(w, seq);

    print("Measuring sequential runs");
    var [seqTimes, seqResult] = measureN(m, seq);
  }

  if (mode(PAR)) {
    print("Warming up parallel runs");
    warmup(w, par);

    print("Measuring parallel runs");
    var [parTimes, parResult] = measureN(m, par);
  }

  if (mode(SEQ|PAR)) {
    // Check correctness
    print("Checking correctness");
    assertStructuralEq(seqResult, parResult);
  }

  if (mode(SEQ)) {
    var seqAvg = average(seqTimes);
    for (var i = 0; i < seqTimes.length; i++)
      print(label + " SEQUENTIAL MEASUREMENT " + i + ": " + seqTimes[i]);
    print(label + " SEQUENTIAL AVERAGE: " + seqAvg);
  }

  if (mode(PAR)) {
    var parAvg = average(parTimes);
    for (var i = 0; i < parTimes.length; i++)
      print(label + " PARALLEL MEASUREMENT " + i + ": " + parTimes[i]);
    print(label + " PARALLEL AVERAGE  : " + parAvg);
  }

  if (mode(SEQ|PAR)) {
    print(label + " SEQ/PAR RATIO     : " + seqAvg/parAvg);
    print(label + " PAR/SEQ RATIO     : " + parAvg/seqAvg);
    print(label + " IMPROVEMENT       : " +
          (((seqAvg - parAvg) / seqAvg * 100.0) | 0) + "%");
  }

  function mode(m) {
    return (bits & m) === m;
  }
}

function measure1(f) {
  var start = new Date();
  result = f();
  var end = new Date();
  return [end.getTime() - start.getTime(), result];
}

function warmup(iters, f) {
  for (var i = 0; i < iters; i++) {
    print(".");
    f();
  }
}

function average(measurements) {
  var sum = measurements.reduce(function (x, y) { return x + y; });
  return sum / measurements.length;
}

function measureN(iters, f) {
  var measurement, measurements = [];
  var result;

  for (var i = 0; i < iters; i++) {
    [measurement, result] = measure1(f);
    measurements.push(measurement);
  }

  return [measurements, result];
}

function assertStructuralEq(e1, e2) {
    if (e1 instanceof ParallelArray && e2 instanceof ParallelArray) {
      assertEqParallelArray(e1, e2);
    } else if (typeof(RectArray) != "undefined" &&
               e1 instanceof ParallelArray && e2 instanceof RectArray) {
      assertEqParallelArrayRectArray(e1, e2);
    } else if (typeof(RectArray) != "undefined" &&
               e1 instanceof RectArray && e2 instanceof ParallelArray) {
      assertEqParallelArrayRectArray(e2, e1);
    } else if (typeof(WrapArray) != "undefined" &&
               e1 instanceof ParallelArray && e2 instanceof WrapArray) {
      assertEqParallelArrayWrapArray(e1, e2);
    } else if (typeof(WrapArray) != "undefined" &&
               e1 instanceof WrapArray && e2 instanceof ParallelArray) {
      assertEqParallelArrayWrapArray(e2, e1);
    } else if (e1 instanceof Array && e2 instanceof ParallelArray) {
      assertEqParallelArrayArray(e2, e1);
    } else if (e1 instanceof ParallelArray && e2 instanceof Array) {
      assertEqParallelArrayArray(e1, e2);
    } else if (typeof(RectArray) != "undefined" &&
               e1 instanceof RectArray && e2 instanceof RectArray) {
      assertEqRectArray(e1, e2);
    } else if (typeof(WrapArray) != "undefined" &&
               e1 instanceof WrapArray && e2 instanceof WrapArray) {
      assertEqWrapArray(e1, e2);
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

function assertEqParallelArrayRectArray(a, b) {
  assertEq(a.shape.length, 2);
  assertEq(a.shape[0], b.width);
  assertEq(a.shape[1], b.height);
  for (var i = 0, w = a.shape[0]; i < w; i++) {
    for (var j = 0, h = a.shape[1]; j < h; j++) {
      assertStructuralEq(a.get(i,j), b.get(i,j));
    }
  }
}

function assertEqParallelArrayWrapArray(a, b) {
  assertEq(a.shape.length, 2);
  assertEq(a.shape[0], b.width);
  assertEq(a.shape[1], b.height);
  for (var i = 0, w = a.shape[0]; i < w; i++) {
    for (var j = 0, h = a.shape[1]; j < h; j++) {
      assertStructuralEq(a.get(i,j), b.get(i,j));
    }
  }
}

function assertEqParallelArrayArray(a, b) {
  assertEq(a.shape.length, 1);
  assertEq(a.length, b.length);
  for (var i = 0, l = a.shape[0]; i < l; i++) {
    assertStructuralEq(a.get(i), b[i]);
  }
}

function assertEqRectArray(a, b) {
  assertEq(a.width, b.width);
  assertEq(a.height, b.height);
  for (var i = 0, w = a.width; i < w; i++) {
    for (var j = 0, h = a.height; j < h; j++) {
      assertStructuralEq(a.get(i,j), b.get(i,j));
    }
  }
}

function assertEqWrapArray(a, b) {
  assertEq(a.width, b.width);
  assertEq(a.height, b.height);
  for (var i = 0, w = a.width; i < w; i++) {
    for (var j = 0, h = a.height; j < h; j++) {
      assertStructuralEq(a.get(i,j), b.get(i,j));
    }
  }
}

function assertEqArray(a, b) {
  assertEq(a.length, b.length);
  for (var i = 0, l = a.length; i < l; i++) {
    assertStructuralEq(a[i], b[i]);
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
    var e1 = a.get.apply(a, iv);
    var e2 = b.get.apply(b, iv);
    assertStructuralEq(e1, e2);
  } while (bump(iv));
}
