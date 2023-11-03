// Test inline lower case conversion of ASCII / Latin-1 strings.

function* characters(...ranges) {
  for (let [start, end] of ranges) {
    for (let i = start; i <= end; ++i) {
      yield i;
    }
  }
}

const ascii_upper = [...characters(
  [0x41, 0x5A], // A..Z
  [0x30, 0x39], // 0..9
)];

const ascii_lower = [...characters(
  [0x61, 0x7A], // a..z
  [0x30, 0x39], // 0..9
)];

const latin1_upper = [...characters(
  [0xC0, 0xDE], // À..Þ
  [0x30, 0x39], // 0..9
)];

const latin1_lower = [...characters(
  [0xDF, 0xFF], // ß..ÿ
)];

for (let upper of [ascii_upper, latin1_upper]) {
  let s = String.fromCodePoint(...upper);
  assertEq(isLatin1(s), true);
  assertEq(s, s.toUpperCase());
}

for (let lower of [ascii_lower, latin1_lower]) {
  let s = String.fromCodePoint(...lower);
  assertEq(isLatin1(s), true);
  assertEq(s, s.toLowerCase());
}

function toRope(s) {
  try {
    return newRope(s[0], s.substring(1));
  } catch {}
  // newRope can fail when |s| fits into an inline string. In that case simply
  // return the input.
  return s;
}

for (let i = 0; i <= 32; ++i) {
  let strings = [ascii_upper, ascii_lower, latin1_upper, latin1_lower].flatMap(codePoints => [
    String.fromCodePoint(...codePoints.slice(0, i)),

    // Trailing ASCII upper case character.
    String.fromCodePoint(...codePoints.slice(0, i)) + "A",

    // Trailing ASCII lower case character.
    String.fromCodePoint(...codePoints.slice(0, i)) + "a",

    // Trailing Latin-1 upper case character.
    String.fromCodePoint(...codePoints.slice(0, i)) + "À",

    // Trailing Latin-1 lower case character.
    String.fromCodePoint(...codePoints.slice(0, i)) + "ß",
  ]).flatMap(x => [
    x,
    toRope(x),
    newString(x, {twoByte: true}),
  ]);

  const expected = strings.map(x => {
    // Prevent Warp compilation when computing the expected results.
    with ({}) ;
    return x.toLowerCase();
  });

  for (let i = 0; i < 1000; ++i) {
    let idx = i % strings.length;
    let str = strings[idx];

    let actual = str.toLowerCase();
    if (actual !== expected[idx]) throw new Error();
  }
}
