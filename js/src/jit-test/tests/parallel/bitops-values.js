load(libdir + "parallelarray-helpers.js");

// The following tests test 2 paths in Ion. The 'ValueToInt32' paths test
// LValueToInt32, and the 'V' paths test the slow value-taking VM functions
// corresponding to the bit ops.
//
// At the time of writing the 'V' paths are triggered when any of the operands
// to the bit ops may be an object, thus the always-false branch which assign
// {} to one of the operands.

var len = minItemsTestingThreshold;
var vals = [true, false, undefined, "1"];

function testBitNotValueToInt32() {
  assertArraySeqParResultsEq(range(0, len), "map", function (i) {
    return [~vals[0], ~vals[1], ~vals[2], ~vals[3]];
  });
}

function testBitNotV() {
  assertArraySeqParResultsEq(range(0, len), "map", function (i) {
    var a, b, c, d;
    if (i < 0) {
      a = {};
      b = {};
      c = {};
      d = {};
    } else {
      a = vals[0];
      b = vals[1];
      c = vals[2];
      d = vals[3];
    }
    return [~a, ~b, ~c, ~d];
  });
}

function testBitBinOpsValueToInt32() {
  for (var n = 0; n < vals.length; n++) {
    for (var m = n; m < vals.length; m++) {
      assertArraySeqParResultsEq(range(0, len), "map", function (i) {
        var a = vals[n];
        var b = vals[m];

        return [a & b,
                a | b,
                a ^ b,
                a << b,
                a >> b];
      });
    }
  }
}

function testBitBinOpsV() {
  for (var n = 0; n < vals.length; n++) {
    for (var m = n; m < vals.length; m++) {
      assertArraySeqParResultsEq(range(0, len), "map", function (i) {
        var a, b;

        if (i < 0) {
          a = {};
          b = {};
        } else {
          a = vals[n];
          b = vals[m];
        }

        return [a & b,
                a | b,
                a ^ b,
                a << b,
                a >> b];
      });
    }
  }
}

if (getBuildConfiguration().parallelJS) {
  testBitNotValueToInt32();
  testBitNotV();
  testBitBinOpsValueToInt32();
  testBitBinOpsV();
}
