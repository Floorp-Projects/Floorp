// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.Intl.Segmenter)

// Sentence boundaries can be locale dependent. The following locales don't use
// any custom tailoring, so they should give the same results.
const locales = [
  "en", "de", "fr", "ar", "ja", "zh", "th",
];

let strings = {
  // Empty string
  "": [],

  // Ascii
  "This is an English sentence. And this is another one.": [
    "This is an English sentence. ",
    "And this is another one."
  ],
  "The colon: it doesn't start a new sentence.": [
    "The colon: it doesn't start a new sentence."
  ],

  // Latin-1
  "Unnötig umständlich Wörter überlegen. Und dann lästigerweise zu längeren Sätzen überarbeiten!": [
    "Unnötig umständlich Wörter überlegen. ",
    "Und dann lästigerweise zu längeren Sätzen überarbeiten!"
  ],

  // Two-Byte
  // Source: https://ja.wikipedia.org/wiki/Unicode
  "Unicode（ユニコード）は、符号化文字集合や文字符号化方式などを定めた、文字コードの業界規格。文字集合（文字セット）が単一の大規模文字セットであること（「Uni」という名はそれに由来する）などが特徴である。": [
    "Unicode（ユニコード）は、符号化文字集合や文字符号化方式などを定めた、文字コードの業界規格。",
    "文字集合（文字セット）が単一の大規模文字セットであること（「Uni」という名はそれに由来する）などが特徴である。"
  ],
};

function assertIsSegmentDataObject(obj) {
  // The prototype is %Object.prototype%.
  assertEq(Object.getPrototypeOf(obj), Object.prototype);

  // The Segment Data object has exactly three own properties.
  let keys = Reflect.ownKeys(obj);
  assertEq(keys.length, 3);
  assertEq(keys[0], "segment");
  assertEq(keys[1], "index");
  assertEq(keys[2], "input");

  // Ensure each property has the correct value type.
  assertEq(typeof obj.segment, "string");
  assertEq(typeof obj.index, "number");
  assertEq(typeof obj.input, "string");

  // |index| is an integer index into |string|.
  assertEq(Number.isInteger(obj.index), true);
  assertEq(obj.index >= 0, true);
  assertEq(obj.index < obj.input.length, true);

  // Segments are non-empty.
  assertEq(obj.segment.length > 0, true);

  // Ensure the segment is present in the input at the correct position.
  assertEq(obj.input.substr(obj.index, obj.segment.length), obj.segment);
}

function segmentsFromContaining(segmenter, string) {
  let segments = segmenter.segment(string);

  let result = [];
  for (let index = 0, data; (data = segments.containing(index)); index += data.segment.length) {
    result.push(data);
  }
  return result;
}

for (let locale of locales) {
  let segmenter = new Intl.Segmenter(locale, {granularity: "sentence"});

  let resolved = segmenter.resolvedOptions();
  assertEq(resolved.locale, locale);
  assertEq(resolved.granularity, "sentence");

  for (let [string, sentences] of Object.entries(strings)) {
    let segments = [...segmenter.segment(string)];

    // Assert each segment is a valid Segment Data object.
    segments.forEach(assertIsSegmentDataObject);

    // Concatenating all segments should return the input.
    assertEq(segments.reduce((acc, {segment}) => acc + segment, ""), string);

    // The "input" property matches the original input string.
    assertEq(segments.every(({input}) => input === string), true);

    // The indices are sorted in ascending order.
    assertEq(isNaN(segments.reduce((acc, {index}) => index > acc ? index : NaN, -Infinity)), false);

    // The computed segments match the expected value.
    assertEqArray(segments.map(({segment}) => segment), sentences);

    // Segment iteration and %Segments.prototype%.containing return the same results.
    assertDeepEq(segmentsFromContaining(segmenter, string), segments);
  }
}

// Sentence break suppressions through the "ss" Unicode extension key aren't supported.
{
  let segmenter = new Intl.Segmenter("en-u-ss-standard", {granularity: "sentence"});
  assertEq(segmenter.resolvedOptions().locale, "en");

  let segments = [...segmenter.segment("Dr. Strange is a fictional character.")];
  assertEqArray(segments.map(({segment}) => segment),
                ["Dr. ", "Strange is a fictional character."]);
}

// Locale-dependent sentence segmentation.
{
  // https://en.wikipedia.org/wiki/Greek_question_mark#Greek_question_mark
  let string1 = "Από πού είσαι; Τί κάνεις;";
  let string2 = string1.replaceAll(";", "\u037E"); // U+037E GREEK QUESTION MARK
  assertEq(string1 !== string2, true);

  for (let string of [string1, string2]) {
    let english = new Intl.Segmenter("en", {granularity: "sentence"});
    let greek = new Intl.Segmenter("el", {granularity: "sentence"});

    // A single sentence in English.
    assertEq([...english.segment(string)].length, 1);

    // But two sentences in Greek.
    //
    // ICU4X doesn't support locale-specific tailoring:
    // https://github.com/unicode-org/icu4x/issues/3284
    // assertEq([...greek.segment(string)].length, 2);
  }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
