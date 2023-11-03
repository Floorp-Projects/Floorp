// Test comparison against single-element character strings.

function makeComparator(code) {
  var str = String.fromCharCode(code);

  return Function("ch", "code", `
    assertEq(ch == "${str}", code == ${code} && ch.length == 1);
    assertEq(ch != "${str}", code != ${code} || ch.length != 1);

    assertEq(ch < "${str}", code < ${code} || (code == ${code} && ch.length < 1));
    assertEq(ch <= "${str}", code < ${code} || (code == ${code} && ch.length <= 1));
    assertEq(ch > "${str}", code > ${code} || (code == ${code} && ch.length > 1));
    assertEq(ch >= "${str}", code > ${code} || (code == ${code} && ch.length >= 1));
  `);
}

function testCompare(strings, comparator) {
  // Don't Ion compile to ensure |comparator| won't be inlined.
  with ({}) ;

  for (let i = 0; i < 1000; ++i) {
    let str = strings[i % strings.length];

    comparator("", -1);

    for (let j = 0; j < str.length; ++j) {
      let ch = str.charAt(j);
      let code = str.charCodeAt(j);

      comparator(ch, code);
      comparator(ch + "A", code);
    }
  }
}

// Compare against the Latin-1 character U+0062 ('b').
testCompare([
  "",
  "a", "b", "c",
  "a-", "b-", "c-",
  "a\u{100}", "b\u{100}", "c\u{100}",
], makeComparator(0x62));

// Compare against the maximum Latin-1 character U+00FF.
testCompare([
  "",
  "a", "b", "c",
  "a-", "b-", "c-",
  "\u{fe}", "\u{ff}", "\u{100}",
  "\u{fe}-", "\u{ff}-", "\u{100}-",
], makeComparator(0xff));

// Compare against the Two-byte character U+0101.
testCompare([
  "",
  "a", "b", "c",
  "a-", "b-", "c-",
  "\u{100}", "\u{101}", "\u{102}",
  "\u{100}-", "\u{101}-", "\u{102}-",
], makeComparator(0x101));
