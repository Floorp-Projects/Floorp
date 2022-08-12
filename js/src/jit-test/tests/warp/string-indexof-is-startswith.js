// |str.indexOf(searchStr) == 0| is compiled as |str.startsWith(searchStr)|.
// |str.indexOf(searchStr) != 0| is compiled as |!str.startsWith(searchStr)|.

const strings = [
  "",
  "a", "b",
  "ab", "ba", "ac", "ca",
  "aba", "abb", "abc", "aca",
];

function StringIndexOf(str, searchStr) {
  // Prevent Warp compilation when computing the expected result.
  with ({});
  return str.indexOf(searchStr);
}

for (let str of strings) {
  for (let searchStr of strings) {
    let startsWith = str.indexOf(searchStr) === 0;

    assertEq(startsWith, str.startsWith(searchStr));
    assertEq(startsWith, StringIndexOf(str, searchStr) === 0);

    let notStartsWith = str.indexOf(searchStr) !== 0;

    assertEq(notStartsWith, !str.startsWith(searchStr));
    assertEq(notStartsWith, StringIndexOf(str, searchStr) !== 0);
  }
}
