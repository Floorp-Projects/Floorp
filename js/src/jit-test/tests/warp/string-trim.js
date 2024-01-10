const whitespace = [
  // Ascii whitespace
  " ",
  "\t",

  // Latin-1 whitespace
  "\u{a0}",

  // Two-byte whitespace
  "\u{1680}",
];

const strings = [
  // Empty string
  "",

  // Ascii strings
  "a",
  "abc",

  // Latin-1 strings
  "á",
  "áèô",

  // Two-byte strings
  "\u{100}",
  "\u{100}\u{101}\u{102}",
].flatMap(x => [
  x,

  // Leading whitespace
  ...whitespace.flatMap(w => [w + x, w + w + x]),

  // Trailing whitespace
  ...whitespace.flatMap(w => [x + w, x + w + w]),

  // Leading and trailing whitespace
  ...whitespace.flatMap(w => [w + x + w, w + w + x + w + w]),

  // Interspersed whitespace
  ...whitespace.flatMap(w => [x + w + x, x + w + w + x]),
]);

function trim(strings) {
  // Compute expected values using RegExp.
  let expected = strings.map(x => x.replace(/(^\s+)|(\s+$)/g, ""));

  for (let i = 0; i < 1000; ++i) {
    let index = i % strings.length;
    assertEq(strings[index].trim(), expected[index]);
  }
}
for (let i = 0; i < 2; ++i) trim(strings);

function trimStart(strings) {
  // Compute expected values using RegExp.
  let expected = strings.map(x => x.replace(/(^\s+)/g, ""));

  for (let i = 0; i < 1000; ++i) {
    let index = i % strings.length;
    assertEq(strings[index].trimStart(), expected[index]);
  }
}
for (let i = 0; i < 2; ++i) trimStart(strings);

function trimEnd(strings) {
  // Compute expected values using RegExp.
  let expected = strings.map(x => x.replace(/(\s+$)/g, ""));

  for (let i = 0; i < 1000; ++i) {
    let index = i % strings.length;
    assertEq(strings[index].trimEnd(), expected[index]);
  }
}
for (let i = 0; i < 2; ++i) trimEnd(strings);
