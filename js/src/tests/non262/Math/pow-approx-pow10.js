if (typeof fdlibm === "undefined") {
  var fdlibm = SpecialPowers.Cu.getJSTestingFunctions().fdlibm;
}

const f64 = new Float64Array(1);
const ui64 = new BigUint64Array(f64.buffer);

function toBits(n) {
  f64[0] = n;
  return ui64[0];
}

function errorInULP(actual, expected) {
  // Handle NaN and +0/-0.
  if (Object.is(actual, expected)) {
    return 0;
  }

  let x = toBits(actual);
  let y = toBits(expected);
  return x <= y ? Number(y - x) : Number(x - y);
}

const maxExponent = Math.trunc(Math.log10(Number.MAX_VALUE));
const minExponent = Math.trunc(Math.log10(Number.MIN_VALUE));

assertEq(Math.pow(10, maxExponent + 1), Infinity);
assertEq(Math.pow(10, minExponent - 1), 0);

// Ensure the error is less than 2 ULP when compared to fdlibm.
for (let i = minExponent; i <= maxExponent; ++i) {
  let actual = Math.pow(10, i);
  let expected = fdlibm.pow(10, i);
  let error = errorInULP(actual, expected);

  assertEq(error < 2, true,
           `${10} ** ${i}: ${actual} (${toBits(actual).toString(16)}) != ${expected} (${toBits(expected).toString(16)})`);
}

// Ensure the error is less than 2 ULP when compared to parsed string |1ep|.
for (let i = minExponent; i <= maxExponent; ++i) {
  let actual = Math.pow(10, i);
  let expected = Number("1e" + i);
  let error = errorInULP(actual, expected);

  assertEq(error < 2, true,
           `${10} ** ${i}: ${actual} (${toBits(actual).toString(16)}) != ${expected} (${toBits(expected).toString(16)})`);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
