// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.Intl.Segmenter)

// https://www.unicode.org/reports/tr29/#Sentence_Boundary_Rules

const strings = {
  // SB1, SB2
  "": [],

  // SB3
  "\r\n": ["\r\n"],

  // SB4
  "First paragraph.\nSecond paragraph.": ["First paragraph.\n", "Second paragraph."],
  "First paragraph.\rSecond paragraph.": ["First paragraph.\r", "Second paragraph."],
  "First paragraph.\r\nSecond paragraph.": ["First paragraph.\r\n", "Second paragraph."],
  "First paragraph.\x85Second paragraph.": ["First paragraph.\x85", "Second paragraph."],

  // SB5
  "\xADWo\xADrd\xAD.\xAD": ["\xADWo\xADrd\xAD.\xAD"],
  "Word.\n\xAD": ["Word.\n", "\xAD"],
  "Word.\r\xAD\n": ["Word.\r", "\xAD\n"],

  // SB6
  ".2": [".2"],
  "1.2": ["1.2"],
  "!2": ["!", "2"],
  "1!2": ["1!", "2"],

  // SB7
  "A.B": ["A.B"],
  "a.B": ["a.B"],
  "A. B": ["A. ", "B"],
  "a. B": ["a. ", "B"],

  // SB8
  "#.a": ["#.a"],
  "#. a": ["#. a"],
  "#. # a": ["#. # a"],
  "#. 1 a": ["#. 1 a"],
  "#. , a": ["#. , a"],
  "#. Aa": ["#. ", "Aa"],

  // SB8a
  "Word..": ["Word.."],
  "Word . , ": ["Word . , "],
  "Word.'\t , ": ["Word.'\t , "],

  // SB9, SB10, SB11
  "Word.''": ["Word.''"],
  "Word.'\t ": ["Word.'\t "],
  "Word.'\t \n": ["Word.'\t \n"],
};

function assertSegments(string, sentences) {
  let seg = segmenter.segment(string);
  let segments = [...seg];

  // The computed segments match the expected value.
  assertEqArray(segments.map(({segment}) => segment), sentences);

  // |containing()| should return the same result.
  for (let expected of segments) {
    let {segment, index} = expected;
    for (let i = index; i < index + segment.length; ++i) {
      let actual = seg.containing(i);
      assertDeepEq(actual, expected);
    }
  }
}

let segmenter = new Intl.Segmenter("en", {granularity: "sentence"});

for (let [string, words] of Object.entries(strings)) {
  assertSegments(string, words);
}

// Locale-dependent sentence segmentation.
{
  // https://en.wikipedia.org/wiki/Greek_question_mark#Greek_question_mark
  let string = "A sentence; semicolon separated.";

  let english = new Intl.Segmenter("en", {granularity: "sentence"});
  let greek = new Intl.Segmenter("el", {granularity: "sentence"});

  // A single sentence in English.
  assertEq([...english.segment(string)].length, 1);

  // ICU4C: Two sentences in Greek.
  // assertEq([...greek.segment(string)].length, 2);

  // ICU4X: A single sentence in Greek.
  assertEq([...greek.segment(string)].length, 1);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
