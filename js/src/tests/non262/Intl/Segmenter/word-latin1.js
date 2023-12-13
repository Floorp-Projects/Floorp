// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.Intl.Segmenter)

// https://www.unicode.org/reports/tr29/#Word_Boundary_Rules

const strings = {
  // WB1, WB2
  "": [],

  // WB3
  "\r\n": ["\r\n"],

  // WB3a, WB3b
  "\n": ["\n"],
  "\r": ["\r"],
  "\v": ["\v"],
  "\f": ["\f"],
  "\x85": ["\x85"],

  // WB3d
  " ": [" "],
  "  ": ["  "],

  // WB4
  "\xAD": ["\xAD"],
  "\xAD\xAD": ["\xAD\xAD"],

  // WB5
  "a": ["a"],
  "ab": ["ab"],

  // WB6, WB7
  "a:b": ["a:b"],
  "a·b": ["a·b"],
  "a.b": ["a.b"],
  "a'b": ["a'b"],

  // WB8
  "1": ["1"],
  "12": ["12"],

  // WB9
  "a1": ["a1"],

  // WB10
  "1a": ["1a"],

  // WB11, WB12
  "1,2": ["1,2"],
  "1;2": ["1;2"],
  "1.2": ["1.2"],
  "1'2": ["1'2"],

  // WB13a
  "a_": ["a_"],
  "1_": ["1_"],
  "__": ["__"],

  // WB13b
  "_a": ["_a"],
  "_1": ["_1"],

  // WB999
  "\0": ["\0"],
  "?": ["?"],
  "??": ["?", "?"],
};

function assertSegments(string, words) {
  let seg = segmenter.segment(string);
  let segments = [...seg];

  // The computed segments match the expected value.
  assertEqArray(segments.map(({segment}) => segment), words);

  // |containing()| should return the same result.
  for (let expected of segments) {
    let {segment, index} = expected;
    for (let i = index; i < index + segment.length; ++i) {
      let actual = seg.containing(i);
      assertDeepEq(actual, expected);
    }
  }
}

let segmenter = new Intl.Segmenter("en", {granularity: "word"});

for (let [string, words] of Object.entries(strings)) {
  assertSegments(string, words);
}

// WB3, WB3a, WB3b and WB4
for (let string of ["\r\n", "\n", "\r", "\v", "\f", "\x85"]) {
  assertSegments(string + "\xAD", [string, "\xAD"]);
  assertSegments("\xAD" + string, ["\xAD", string]);
}

// WB3d and WB4
for (let string of [" ", "  "]) {
  assertSegments(string + "\xAD", [string + "\xAD"]);
  assertSegments("\xAD" + string, ["\xAD", string]);
}
assertSegments(" \xAD ", [" \xAD", " "]);
assertSegments(" \xAD\xAD ", [" \xAD\xAD", " "]);

// WB5-WB13 and WB4
for (let string of [
  // WB5
  "a", "ab",

  // WB6, WB7
  "a:b",
  "a·b",
  "a.b",
  "a'b",

  // WB8
  "1",
  "12",

  // WB9
  "a1",

  // WB10
  "1a",

  // WB11, WB12
  "1,2",
  "1;2",
  "1.2",
  "1'2",

  // WB13a
  "a_",
  "1_",
  "__",

  // WB13b
  "_a",
  "_1",

  // WB999
  "?",
]) {
  assertSegments(string + "\xAD", [string + "\xAD"]);
  assertSegments("\xAD" + string, ["\xAD", string]);

  if (string === "a.b") {
    // ICU4X incorrectly splits the result into three words.
    // https://github.com/unicode-org/icu4x/issues/4417
    assertSegments(string.split("").join("\xAD"), ["a\xAD", ".\xAD", "b"]);
    assertSegments(string.split("").join("\xAD\xAD"), ["a\xAD\xAD", ".\xAD\xAD", "b"]);
  } else {
    assertSegments(string.split("").join("\xAD"), [string.split("").join("\xAD")]);
    assertSegments(string.split("").join("\xAD\xAD"), [string.split("").join("\xAD\xAD")]);
  }
}

assertSegments("?\xAD?", ["?\xAD", "?"]);

for (let string of [
  // WB6, WB7
  "a:b",
  "a·b",
  "a.b",
  "a'b",

  // WB11, WB12
  "1,2",
  "1;2",
  "1.2",
  "1'2",
]) {
  let prefix = string.slice(0, -1);
  let suffix = string.slice(1);

  assertSegments(prefix, prefix.split(""));
  assertSegments(suffix, suffix.split(""));
}

// MidNum with ALetter
assertSegments("a,b", ["a", ",", "b"]);
assertSegments("a;b", ["a", ";", "b"]);

// MidLetter with Numeric
assertSegments("1:2", ["1", ":", "2"]);
assertSegments("1·2", ["1", "·", "2"]);

// MidNumLet with mixed ALetter and Numeric
assertSegments("a.2", ["a", ".", "2"]);
assertSegments("1.b", ["1", ".", "b"]);
assertSegments("a'2", ["a", "'", "2"]);
assertSegments("1'b", ["1", "'", "b"]);

// MidNum with ExtendNumLet
assertSegments("_,_", ["_", ",", "_"]);
assertSegments("_;_", ["_", ";", "_"]);

// MidLetter with ExtendNumLet
assertSegments("_:_", ["_", ":", "_"]);
assertSegments("_·_", ["_", "·", "_"]);

// MidNumLet with ExtendNumLet
assertSegments("_._", ["_", ".", "_"]);
assertSegments("_'_", ["_", "'", "_"]);

// CLDR has locale-dependent word segmentation for the "en-posix" locale. This
// locale is currently not selectable, so the Latin-1 fast-paths don't need to
// implement it. If one of the two below assertions ever fail, please update
// the Latin-1 fast-paths for word segmentation to implement the "en-posix"
// changes.
assertEq(new Intl.Segmenter("en-posix").resolvedOptions().locale, "en");
assertEq(new Intl.Segmenter("en-u-va-posix").resolvedOptions().locale, "en");

if (typeof reportCompare === "function")
  reportCompare(0, 0);
