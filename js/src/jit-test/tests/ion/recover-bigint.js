// |jit-test| --ion-limit-script-size=off

setJitCompilerOption("baseline.warmup.trigger", 9);
setJitCompilerOption("ion.warmup.trigger", 20);

// Prevent the GC from cancelling Ion compilations, when we expect them to succeed
gczeal(0);

// Keep in sync with BigInt::MaxBitLength.
const maxBitLength = 1024 * 1024;

const maxBigInt = BigInt.asUintN(maxBitLength, -1n);
const minBigInt = -maxBigInt;

function resumeHere() {}

function bigIntAddBail(i) {
  var x = [0n, maxBigInt][0 + (i >= 99)];

  var a = x + 1n;

  // Add a function call to capture a resumepoint at the end of the call or
  // inside the inlined block, such as the bailout does not rewind to the
  // beginning of the function.
  resumeHere();

  if (i >= 99) {
    bailout();
    // Flag the computation as having removed uses to check that we properly
    // report the error while executing the BigInt operation on bailout.
    return a;
  }
}

function bigIntSubBail(i) {
  var x = [0n, minBigInt][0 + (i >= 99)];

  var a = x - 1n;

  // Add a function call to capture a resumepoint at the end of the call or
  // inside the inlined block, such as the bailout does not rewind to the
  // beginning of the function.
  resumeHere();

  if (i >= 99) {
    bailout();
    // Flag the computation as having removed uses to check that we properly
    // report the error while executing the BigInt operation on bailout.
    return a;
  }
}

function bigIntMulBail(i) {
  var x = [0n, maxBigInt][0 + (i >= 99)];

  var a = x * 2n;

  // Add a function call to capture a resumepoint at the end of the call or
  // inside the inlined block, such as the bailout does not rewind to the
  // beginning of the function.
  resumeHere();

  if (i >= 99) {
    bailout();
    // Flag the computation as having removed uses to check that we properly
    // report the error while executing the BigInt operation on bailout.
    return a;
  }
}

function bigIntIncBail(i) {
  var x = [0n, maxBigInt][0 + (i >= 99)];

  var a = x++;

  // Add a function call to capture a resumepoint at the end of the call or
  // inside the inlined block, such as the bailout does not rewind to the
  // beginning of the function.
  resumeHere();

  if (i >= 99) {
    bailout();
    // Flag the computation as having removed uses to check that we properly
    // report the error while executing the BigInt operation on bailout.
    return x;
  }
}

function bigIntDecBail(i) {
  var x = [0n, minBigInt][0 + (i >= 99)];

  var a = x--;

  // Add a function call to capture a resumepoint at the end of the call or
  // inside the inlined block, such as the bailout does not rewind to the
  // beginning of the function.
  resumeHere();

  if (i >= 99) {
    bailout();
    // Flag the computation as having removed uses to check that we properly
    // report the error while executing the BigInt operation on bailout.
    return x;
  }
}

function bigIntBitNotBail(i) {
  var x = [0n, maxBigInt][0 + (i >= 99)];

  var a = ~x;

  // Add a function call to capture a resumepoint at the end of the call or
  // inside the inlined block, such as the bailout does not rewind to the
  // beginning of the function.
  resumeHere();

  if (i >= 99) {
    bailout();
    // Flag the computation as having removed uses to check that we properly
    // report the error while executing the BigInt operation on bailout.
    return a;
  }
}

function bigIntLshBail(i) {
  var shift = [0n, BigInt(maxBitLength)][0 + (i >= 99)];

  var a = 1n << shift;

  // Add a function call to capture a resumepoint at the end of the call or
  // inside the inlined block, such as the bailout does not rewind to the
  // beginning of the function.
  resumeHere();

  if (i >= 99) {
    bailout();
    // Flag the computation as having removed uses to check that we properly
    // report the error while executing the BigInt operation on bailout.
    return a;
  }
}

function bigIntRshBail(i) {
  var shift = [0n, -BigInt(maxBitLength)][0 + (i >= 99)];

  var a = 1n >> shift;

  // Add a function call to capture a resumepoint at the end of the call or
  // inside the inlined block, such as the bailout does not rewind to the
  // beginning of the function.
  resumeHere();

  if (i >= 99) {
    bailout();
    // Flag the computation as having removed uses to check that we properly
    // report the error while executing the BigInt operation on bailout.
    return a;
  }
}

function bigIntAsUintBail(i) {
  var x = [0, maxBitLength + 1][0 + (i >= 99)];

  var a = BigInt.asUintN(x, -1n);

  // Add a function call to capture a resumepoint at the end of the call or
  // inside the inlined block, such as the bailout does not rewind to the
  // beginning of the function.
  resumeHere();

  if (i >= 99) {
    bailout();
    // Flag the computation as having removed uses to check that we properly
    // report the error while executing the BigInt operation on bailout.
    return a;
  }
}

// Prevent compilation of the top-level
eval(`(${resumeHere})`);

// The bigIntXBail() functions create a BigInt which exceeds the maximum
// representable BigInt. This results in either throwing a RangeError or an
// out-of-memory error when the operation is recovered during a bailout.

try {
  for (let i = 0; i < 100; i++) {
    bigIntAddBail(i);
  }
  throw new Error("missing exception");
} catch (e) {
  assertEq(e instanceof RangeError || e === "out of memory", true, String(e));
}

try {
  for (let i = 0; i < 100; i++) {
    bigIntSubBail(i);
  }
  throw new Error("missing exception");
} catch (e) {
  assertEq(e instanceof RangeError || e === "out of memory", true, String(e));
}

try {
  for (let i = 0; i < 100; i++) {
    bigIntMulBail(i);
  }
  throw new Error("missing exception");
} catch (e) {
  assertEq(e instanceof RangeError || e === "out of memory", true, String(e));
}

try {
  for (let i = 0; i < 100; i++) {
    bigIntIncBail(i);
  }
  throw new Error("missing exception");
} catch (e) {
  assertEq(e instanceof RangeError || e === "out of memory", true, String(e));
}

try {
  for (let i = 0; i < 100; i++) {
    bigIntDecBail(i);
  }
  throw new Error("missing exception");
} catch (e) {
  assertEq(e instanceof RangeError || e === "out of memory", true, String(e));
}

try {
  for (let i = 0; i < 100; i++) {
    bigIntBitNotBail(i);
  }
  throw new Error("missing exception");
} catch (e) {
  assertEq(e instanceof RangeError || e === "out of memory", true, String(e));
}

try {
  for (let i = 0; i < 100; i++) {
    bigIntLshBail(i);
  }
  throw new Error("missing exception");
} catch (e) {
  assertEq(e instanceof RangeError || e === "out of memory", true, String(e));
}

try {
  for (let i = 0; i < 100; i++) {
    bigIntRshBail(i);
  }
  throw new Error("missing exception");
} catch (e) {
  assertEq(e instanceof RangeError || e === "out of memory", true, String(e));
}

try {
  for (let i = 0; i < 100; i++) {
    bigIntAsUintBail(i);
  }
  throw new Error("missing exception");
} catch (e) {
  assertEq(e instanceof RangeError || e === "out of memory", true, String(e));
}
