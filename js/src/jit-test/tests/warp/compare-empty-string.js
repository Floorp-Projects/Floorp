// Test case to cover empty string comparison folding.
//
// MCompare can fold comparison with an empty string constant and replace it
// with |string.length <op> 0|.

const strings = [
  // Zero length string.
  "",

  // Uncommon zero length strings.
  newString("", {external: true}),

  // Latin-1 strings.
  "a",
  "ä",
  "monkey",

  // Two-byte strings.
  "猿",
  "🐒",
  newString("monkey", {twoByte: true}),
];

const operators = [
  "==", "===", "!=", "!==",
  "<", "<=", ">=", ">",
];

for (let op of operators) {
  let lhs = x => `${x} ${op} ""`;
  let rhs = x => `"" ${op} ${x}`;

  for (let input of [lhs, rhs]) {
    let fn = Function("strings", `
      const expected = strings.map(x => {
        // Prevent Warp compilation when computing the expected results.
        with ({}) ;
        return ${input("x")};
      });

      for (let i = 0; i < 200; ++i) {
        let idx = i % strings.length;
        let str = strings[idx];
        let res = ${input("str")};
        assertEq(res, expected[idx]);
      }
    `);
    fn(strings);
  }
}
