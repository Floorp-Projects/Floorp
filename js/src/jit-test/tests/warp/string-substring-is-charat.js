// |str.substring(0, 1)| is compiled as |str.charAt(0)|.

const strings = [
  "",
  "a", "b",
  "ab", "ba",
];

for (let i = 0; i < 1000; ++i) {
  let str = strings[i % strings.length];

  assertEq(str.substring(0, 1), str.charAt(0));
}
