// Test case to cover constant atom guards.
//
// GuardSpecificAtom for short (≤32 characters) constant atoms is optimised.

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

function toRope(s) {
  try {
    return newRope(s[0], s.substring(1));
  } catch {}
  // newRope can fail when |s| fits into an inline string. In that case simply
  // return the input.
  return s;
}

function atomize(s) {
  return Object.keys({[s]: 0})[0];
}

for (let i = 1; i <= 32; ++i) {
  let strings = [ascii, latin1, twoByte].flatMap(codePoints => [
    // Same string as the input.
    String.fromCodePoint(...codePoints.slice(0, i)),

    // Same length as the input, but a different string.
    String.fromCodePoint(...codePoints.slice(1, i + 1)),

    // Shorter string than the input.
    String.fromCodePoint(...codePoints.slice(0, i - 1)),

    // Longer string than the input.
    String.fromCodePoint(...codePoints.slice(0, i + 1)),
  ]).flatMap(x => [
    x,
    toRope(x),
    newString(x, {twoByte: true}),
    atomize(x),
  ]);

  // Must be small enough to transition to megamorphic ICs.
  const stringsPerLoop = 4;

  for (let codePoints of [ascii, latin1, twoByte]) {
    let str = String.fromCodePoint(...codePoints.slice(0, i));

    for (let i = 0; i < strings.length; i += stringsPerLoop) {
      let fn = Function("strings", `
        var obj = {["${str}"]: 0};

        for (let i = 0; i < 250; ++i) {
          let idx = i % strings.length;
          let str = strings[idx];
      
          let actual = str in obj;
          let expected = str === "${str}";
          if (actual !== expected) throw new Error();
        }
      `);
      fn(strings.slice(i, stringsPerLoop));
    }
  }
}
