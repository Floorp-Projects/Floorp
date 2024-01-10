// |str.char(idx) == "b"| is compiled as |str.charCodeAt(idx) == 0x62|.

const strings = [
  "",
  "a", "b", "c",
  "a-", "b-", "c-",
];

for (let i = 0; i < 1000; ++i) {
  let str = strings[i % strings.length];

  // |j <= str.length| to test out-of-bounds access, too.
  for (let j = 0; j <= str.length; ++j) {
    let ch = str.charAt(j);
    let code = j < str.length ? str.charCodeAt(j) : -1;

    assertEq(ch == "", code == -1);

    assertEq(ch == "b", code == 0x62);
    assertEq(ch != "b", code != 0x62);

    assertEq(ch < "b", code < 0x62);
    assertEq(ch <= "b", code <= 0x62);

    assertEq(ch > "b", code > 0x62);
    assertEq(ch >= "b", code >= 0x62);
  }
}
