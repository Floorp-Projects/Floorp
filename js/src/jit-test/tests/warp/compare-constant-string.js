// Test case to cover constant string comparison.
//
// Equality comparison for short (≤32 characters), Latin-1 constant strings is
// optimised during lowering.

function* characters(...ranges) {
  for (let [start, end] of ranges) {
    for (let i = start; i <= end; ++i) {
      yield i;
    }
  }
}

const ascii = [...characters(
  [0x41, 0x5A], // A..Z
  [0x61, 0x7A], // a..z
  [0x30, 0x39], // 0..9
)];

const latin1 = [...characters(
  [0xC0, 0xFF], // À..ÿ
)];

const twoByte = [...characters(
  [0x100, 0x17E], // Ā..ž
)];

function atomize(s) {
  return Object.keys({[s]: 0})[0];
}

const operators = [
  "==", "===", "!=", "!==",
];

for (let i = 1; i <= 32; ++i) {
  let strings = [ascii, latin1, twoByte].flatMap(codePoints => [
    // Same string as the input.
    String.fromCodePoint(...codePoints.slice(0, i)),

    // Same length as the input, but a different string.
    String.fromCodePoint(...codePoints.slice(1, i + 1)),

    // Shorter string than the input.
    String.fromCodePoint(...codePoints.slice(0, i - 1)),

    // Longer string than the input.
    String.fromCodePoint(...codePoints.slice(0, i + 1)),
  ]).flatMap(x => [
    x,
    newRope(x, ""),
    newString(x, {twoByte: true}),
    atomize(x),
  ]);

  for (let codePoints of [ascii, latin1, twoByte]) {
    let str = String.fromCodePoint(...codePoints.slice(0, i));

    for (let op of operators) {
      let fn = Function("strings", `
        const expected = strings.map(x => {
          // Prevent Warp compilation when computing the expected results.
          with ({}) ;
          return x ${op} "${str}";
        });

        for (let i = 0; i < 250; ++i) {
          let idx = i % strings.length;
          let str = strings[idx];

          let lhs = str ${op} "${str}";
          if (lhs !== expected[idx]) throw new Error();

          let rhs = "${str}" ${op} str;
          if (rhs !== expected[idx]) throw new Error();
        }
      `);
      fn(strings);
    }
  }
}
