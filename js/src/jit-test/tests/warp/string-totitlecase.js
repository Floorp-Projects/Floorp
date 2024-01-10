// Test inline title case conversion.

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

String.prototype.toTitleCase = function() {
  "use strict";

  var s = String(this);

  if (s.length === 0) {
    return s;
  }
  return s[0].toUpperCase() + s.substring(1);
};

function toRope(s) {
  try {
    return newRope(s[0], s.substring(1));
  } catch {}
  // newRope can fail when |s| fits into an inline string. In that case simply
  // return the input.
  return s;
}

for (let i = 0; i <= 32; ++i) {
  let strings = [ascii, latin1, twoByte].flatMap(codePoints => [
    String.fromCodePoint(...codePoints.slice(0, i)),

    // Leading ASCII upper case character.
    "A" + String.fromCodePoint(...codePoints.slice(0, i)),

    // Leading ASCII lower case character.
    "a" + String.fromCodePoint(...codePoints.slice(0, i)),

    // Leading Latin-1 upper case character.
    "À" + String.fromCodePoint(...codePoints.slice(0, i)),

    // Leading Latin-1 lower case character.
    "à" + String.fromCodePoint(...codePoints.slice(0, i)),

    // Leading Two-Byte upper case character.
    "Ā" + String.fromCodePoint(...codePoints.slice(0, i)),

    // Leading Two-Byte lower case character.
    "ā" + String.fromCodePoint(...codePoints.slice(0, i)),
  ]).flatMap(x => [
    x,
    toRope(x),
    newString(x, {twoByte: true}),
  ]);

  const expected = strings.map(x => {
    // Prevent Warp compilation when computing the expected results.
    with ({}) ;
    return x.toTitleCase();
  });

  for (let i = 0; i < 1000; ++i) {
    let idx = i % strings.length;
    let str = strings[idx];

    let actual = str.toTitleCase();
    if (actual !== expected[idx]) throw new Error();
  }
}
