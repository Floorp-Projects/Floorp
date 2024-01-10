// Test case to cover String.prototype.{indexOf,lastIndexOf,includes} with a constant string of length one.

const strings = [
  // Empty string.
  "",

  // Latin-1 string.
  "abcdefgh",

  // Two-byte string.
  "\u{101}\u{102}\u{103}\u{104}\u{105}\u{106}\u{107}\u{108}",
].flatMap(x => [
  x,

  // Add leading characters.
  "!".repeat(10) + x,

  // Add trailing characters.
  x + "!".repeat(10),
]).flatMap(x => [
  x,

  // To cover the case when the string is two-byte, but has only Latin-1 contents.
  newString(x, {twoByte: true}),
]);

const searchStrings = [
  // Latin-1 search strings:
  // - at the start of the input string
  "a",
  // - in the middle of the input string
  "d",
  // - at the end of the input string
  "h",
  // - not in the input string
  "z",

  // Two-byte search strings:
  // - at the start of the input string
  "\u{101}",
  // - in the middle of the input string
  "\u{104}",
  // - at the end of the input string
  "\u{108}",
  // - not in the input string
  "\u{1000}",
];

const stringFunctions = [
  "indexOf",
  "lastIndexOf",
  "includes",
];

for (let stringFunction of stringFunctions) {
  for (let searchString of searchStrings) {
    let fn = Function("strings", `
      const expected = strings.map(x => {
        // Prevent Warp compilation when computing the expected results.
        with ({}) ;
        return x.${stringFunction}("${searchString}");
      });

      for (let i = 0; i < 500; ++i) {
        let idx = i % strings.length;
        let str = strings[idx];

        let actual = str.${stringFunction}("${searchString}");
        assertEq(actual, expected[idx]);
      }
    `);
    fn(strings);
  }
}
