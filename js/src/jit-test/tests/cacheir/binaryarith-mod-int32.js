setJitCompilerOption("ion.forceinlineCaches", 1);

function runTest(dividend, divisor) {
  function test(dividend, divisor, expected) {
    for (var i = 0; i < dividend.length; i++) {
      var x = dividend[i];
      for (var j = 0; j < divisor.length; j++) {
        var y = divisor[j];
        var result = x % y;

        assertEq(result, expected[i * divisor.length + j]);
      }
    }
  }

  var f = Function(`return ${test}`)();

  // Seed the IC to ensure we're using Int32Mod.
  var ones = Array(8).fill(1);
  var zeros = Array(8 * 8).fill(0);
  for (var i = 0; i < 2; ++i) {
    f(ones, ones, zeros);
  }

  var expected = dividend.map(x => divisor.map(y => x % y)).flat();
  for (var i = 0; i < 10; ++i) {
    f(dividend, divisor, expected);
  }
}

const positiveInt32 = [
  1, 2, 3, 4, 5, 6, 7, 8,
  2**31 - 8,
  2**31 - 7,
  2**31 - 6,
  2**31 - 5,
  2**31 - 4,
  2**31 - 3,
  2**31 - 2,
  2**31 - 1,
];

const negativeInt32 = [
  -1, -2, -3, -4, -5, -6, -7, -8,
  -(2**31 - 8),
  -(2**31 - 7),
  -(2**31 - 6),
  -(2**31 - 5),
  -(2**31 - 4),
  -(2**31 - 3),
  -(2**31 - 2),
  -(2**31 - 1),
  -(2**31),
];

const zero = [0];

const cases = [positiveInt32, zero, negativeInt32];

for (let a of cases) {
  for (let b of cases) {
    runTest(a, b);
  }
}

// Test some "interesting" cases.

// |-2147483648 % -1| may internally compute |-2147483648 / -1|, which isn't an int32 and
// may trigger a floating point exception on some architectures.
runTest([-2147483648], [-1]);
