// `str.substring(...)` can return static strings.

const strings = [
  "abcdef",
  "ABCDEF",
];

for (let i = 0; i < 500; ++i) {
  let str = strings[i & 1];

  for (let j = 0; j < 2; ++j) {
    // One element static string.
    let r = str.substring(j, j + 1);
    assertEq(r, str.charAt(j));

    // Two elements static string.
    let s = str.substring(j, j + 2);
    assertEq(s, str.charAt(j) + str.charAt(j + 1));
  }
}
