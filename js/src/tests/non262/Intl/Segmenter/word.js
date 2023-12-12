// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.Intl.Segmenter)

// Word boundaries are locale independent. Test with various locales to ensure
// we get the same results.
const locales = [
  "en", "de", "fr", "ar", "ja", "zh", "th",
];

let strings = {
  // Empty string
  "": [],

  // Ascii
  "This is an English sentence.": [
    "This", " ", "is", " ", "an", " ", "English", " ", "sentence", "."
  ],
  "Moi?  N'est-ce pas.": [
    "Moi", "?", "  ", "N'est", "-", "ce", " ", "pas", "."
  ],

  // Latin-1
  "Unnötig umständlich Wörter überlegen.": [
    "Unnötig", " ", "umständlich", " ", "Wörter", " ", "überlegen", "."
  ],

  // Two-Byte
  // Source: https://en.wikipedia.org/wiki/Japanese_writing_system#Examples
  "ラドクリフ、マラソン五輪代表に 1万メートル出場にも含み。": [
    "ラドクリフ", "、", "マラソン", "五輪", "代表", "に", " ", "1", "万", "メートル", "出場", "に", "も", "含み", "。"
  ],

  // From: Language Sense and Ambiguity in Thai
  // Source: https://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.98.118
  "ขนบนอก": [
    // According to the paper this should instead be separated into ขน|บน|อก.
    "ขนบ", "นอก"
  ],
  "พนักงานนําโคลงเรือสามตัว": [
    // Expected segmentation is พนักงาน|นํา|โค|ลง|เรือ|สาม|ตัว.

    // ICU4C segmentation:
    // "พนัก", "งาน", "นํา", "โคลง", "เรือ", "สาม", "ตัว"

    // ICU4X segmentation:
    "พ", "นัก", "งานนํา", "โคลง", "เรือ", "สาม", "ตัว"
  ],

  "หมอหุงขาวสวยด": [
    // Has three possible segmentations:
    // หมอหงขาว|สวย|ด
    // หมอ|หง|ขาวสวย|ด
    // หมอ|หง|ขาว|สวย|ด

    // ICU4C segmentation:
    // "หมอ", "หุง", "ขาว", "สวย", "ด"

    // ICU4X segmentation:
    "หมอ", "หุง", "ขาว", "สวยด"
  ],

  // From: Thoughts on Word and Sentence Segmentation in Thai
  // Source: https://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.63.7038
  "หนังสือรวมบทความทางวิชาการในการประชุมสัมมนา": [
    "หนังสือ", "รวม", "บทความ", "ทาง", "วิชาการ", "ใน", "การ", "ประชุม", "สัมมนา"
  ],
};

function assertIsSegmentDataObject(obj) {
  // The prototype is %Object.prototype%.
  assertEq(Object.getPrototypeOf(obj), Object.prototype);

  // The Segment Data object has exactly four own properties.
  let keys = Reflect.ownKeys(obj);
  assertEq(keys.length, 4);
  assertEq(keys[0], "segment");
  assertEq(keys[1], "index");
  assertEq(keys[2], "input");
  assertEq(keys[3], "isWordLike");

  // Ensure each property has the correct value type.
  assertEq(typeof obj.segment, "string");
  assertEq(typeof obj.index, "number");
  assertEq(typeof obj.input, "string");
  assertEq(typeof obj.isWordLike, "boolean");

  // |index| is an integer index into |string|.
  assertEq(Number.isInteger(obj.index), true);
  assertEq(obj.index >= 0, true);
  assertEq(obj.index < obj.input.length, true);

  // Segments are non-empty.
  assertEq(obj.segment.length > 0, true);

  // Ensure the segment is present in the input at the correct position.
  assertEq(obj.input.substr(obj.index, obj.segment.length), obj.segment);

  // The non-word parts in the samples are either punctuators or space separators.
  let expectedWordLike = !/^(\p{gc=P}|\p{gc=Zs})+$/u.test(obj.segment);

  // ICU4X incorrectly marks the last segment as non-word like for Thai.
  // https://github.com/unicode-org/icu4x/issues/4446
  let isThai = /^\p{sc=Thai}+$/u.test(obj.segment);
  let isLastSegment = obj.index + obj.segment.length === obj.input.length;
  if (isThai && isLastSegment) {
    expectedWordLike = false;
  }

  assertEq(obj.isWordLike, expectedWordLike, obj.segment);
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
  let segmenter = new Intl.Segmenter(locale, {granularity: "word"});

  let resolved = segmenter.resolvedOptions();
  assertEq(resolved.locale, locale);
  assertEq(resolved.granularity, "word");

  for (let [string, words] of Object.entries(strings)) {
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
    assertEqArray(segments.map(({segment}) => segment), words);

    // Segment iteration and %Segments.prototype%.containing return the same results.
    assertDeepEq(segmentsFromContaining(segmenter, string), segments);
  }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
