// Helpers for Web Audio tests

function expectException(func, exceptionCode) {
  var threw = false;
  try {
    func();
  } catch (ex) {
    threw = true;
    ok(ex instanceof DOMException, "Expect a DOM exception");
    is(ex.code, exceptionCode, "Expect the correct exception code");
  }
  ok(threw, "The exception was thrown");
}

function expectTypeError(func) {
  var threw = false;
  try {
    func();
  } catch (ex) {
    threw = true;
    ok(ex instanceof TypeError, "Expect a TypeError");
  }
  ok(threw, "The exception was thrown");
}

function fuzzyCompare(a, b) {
  return Math.abs(a - b) < 1e-5;
}

function compareBuffers(buf1, buf2,
                        /*optional*/ offset,
                        /*optional*/ length,
                        /*optional*/ sourceOffset,
                        /*optional*/ destOffset) {
  is(buf1.length, buf2.length, "Buffers must have the same length");
  if (length == undefined) {
    length = buf1.length - (offset || 0);
  }
  sourceOffset = sourceOffset || 0;
  destOffset = destOffset || 0;
  var difference = 0;
  var maxDifference = 0;
  var firstBadIndex = -1;
  for (var i = offset || 0; i < Math.min(buf1.length, (offset || 0) + length); ++i) {
    if (!fuzzyCompare(buf1[i + sourceOffset], buf2[i + destOffset])) {
      console.log(buf1[i+sourceOffset] + " " + buf2[i+destOffset]);
      difference++;
      maxDifference = Math.max(maxDifference, Math.abs(buf1[i + sourceOffset] - buf2[i + destOffset]));
      if (firstBadIndex == -1) {
        firstBadIndex = i;
      }
    }
  };

  is(difference, 0, "Found " + difference + " different samples, maxDifference: " +
     maxDifference + ", first bad index: " + firstBadIndex +
     " with source offset " + sourceOffset + " and desitnation offset " +
     destOffset);
}
