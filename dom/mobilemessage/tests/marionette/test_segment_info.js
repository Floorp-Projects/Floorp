/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const LEN_7BIT = 160;
const LEN_7BIT_WITH_8BIT_REF = 153;
const LEN_7BIT_WITH_16BIT_REF = 152;
const LEN_UCS2 = 70;
const LEN_UCS2_WITH_8BIT_REF = 67;
const LEN_UCS2_WITH_16BIT_REF = 66;

function times(str, n) {
  return (new Array(n + 1)).join(str);
}

function test(text, segments, charsPerSegment, charsAvailableInLastSegment) {
  // 'text' may contain non-ascii characters, so we're not going to print it on
  // Marionette console to avoid breaking it.
  ok(true, "Testing '" + text + "' ...");

  let domRequest = manager.getSegmentInfoForText(text);
  ok(domRequest, "DOMRequest object returned.");

  return domRequest.then(function(aResult) {
    ok(aResult, "result = " + JSON.stringify(aResult));

    is(aResult.segments, segments, "result.segments");
    is(aResult.charsPerSegment, charsPerSegment, "result.charsPerSegment");
    is(aResult.charsAvailableInLastSegment, charsAvailableInLastSegment,
       "result.charsAvailableInLastSegment");
  });
}

startTestCommon(function() {
  // Ensure we always begin with strict 7bit encoding set to false.
  return pushPrefEnv({ set: [["dom.sms.strict7BitEncoding", false]] })

    // GSM 7Bit Alphabets:
    //
    // 'a' is in GSM default locking shift table, so it takes 1 septet.
    .then(() => test("a",       1, LEN_7BIT, LEN_7BIT - 1))
    // '\u20ac' is in GSM default single shift table, so it takes 2 septets.
    .then(() => test("\u20ac",  1, LEN_7BIT, LEN_7BIT - 2))
    // SP is defined in both locking shift and single shift tables.
    .then(() => test(" ",       1, LEN_7BIT, LEN_7BIT - 1))
    // Some combinations.
    .then(() => test("a\u20ac", 1, LEN_7BIT, LEN_7BIT - 3))
    .then(() => test("a ",      1, LEN_7BIT, LEN_7BIT - 2))
    .then(() => test("\u20aca", 1, LEN_7BIT, LEN_7BIT - 3))
    .then(() => test("\u20ac ", 1, LEN_7BIT, LEN_7BIT - 3))
    .then(() => test(" \u20ac", 1, LEN_7BIT, LEN_7BIT - 3))
    .then(() => test(" a",      1, LEN_7BIT, LEN_7BIT - 2))

    // GSM 7Bit Alphabets (multipart):
    //
    // Exactly 160 locking shift table chararacters.
    .then(() => test(times("a", LEN_7BIT), 1, LEN_7BIT, 0))
    // 161 locking shift table chararacters. We'll have |161 - 153 = 8| septets in
    // the 2nd segment.
    .then(() => test(times("a", LEN_7BIT + 1),
                     2, LEN_7BIT_WITH_8BIT_REF, LEN_7BIT_WITH_8BIT_REF - 8))
    // |LEN_7BIT_WITH_8BIT_REF * 2| locking shift table chararacters.
    .then(() => test(times("a", LEN_7BIT_WITH_8BIT_REF * 2),
                     2, LEN_7BIT_WITH_8BIT_REF, 0))
    // |LEN_7BIT_WITH_8BIT_REF * 2 + 1| locking shift table chararacters.
    .then(() => test(times("a", LEN_7BIT_WITH_8BIT_REF * 2 + 1),
                     3, LEN_7BIT_WITH_8BIT_REF, LEN_7BIT_WITH_8BIT_REF - 1))
    // Exactly 80 single shift table chararacters.
    .then(() => test(times("\u20ac", LEN_7BIT / 2), 1, LEN_7BIT, 0))
    // 81 single shift table chararacters. Because |Math.floor(153 / 2) = 76|, it
    // should left 5 septets in the 2nd segment.
    .then(() => test(times("\u20ac", LEN_7BIT / 2 + 1),
                     2, LEN_7BIT_WITH_8BIT_REF, LEN_7BIT_WITH_8BIT_REF - 10))
    // |1 + 2 * 76| single shift table chararacters. We have only |153 - 76 * 2 = 1|
    // space left, but each single shift table character takes 2, so it will be
    // filled in the 3rd segment.
    .then(() => test(times("\u20ac", 1 + 2 * Math.floor(LEN_7BIT_WITH_8BIT_REF / 2)),
                     3, LEN_7BIT_WITH_8BIT_REF, LEN_7BIT_WITH_8BIT_REF - 2))
    // |2 * 76| single shift table chararacters + 1 locking shift table chararacter.
    .then(() => test("a" + times("\u20ac", 2 * Math.floor(LEN_7BIT_WITH_8BIT_REF / 2)),
                     2, LEN_7BIT_WITH_8BIT_REF, 1))
    .then(() => test(times("\u20ac", 2 * Math.floor(LEN_7BIT_WITH_8BIT_REF / 2)) + "a",
                     2, LEN_7BIT_WITH_8BIT_REF, 0))

    // UCS2:
    //
    // '\u6afb' should be encoded as UCS2.
    .then(() => test("\u6afb",       1, LEN_UCS2, LEN_UCS2 - 1))
    // Combination of GSM 7bit alphabets.
    .then(() => test("\u6afba",      1, LEN_UCS2, LEN_UCS2 - 2))
    .then(() => test("\u6afb\u20ac", 1, LEN_UCS2, LEN_UCS2 - 2))
    .then(() => test("\u6afb ",      1, LEN_UCS2, LEN_UCS2 - 2))

    // UCS2 (multipart):
    //
    // Exactly 70 UCS2 chararacters.
    .then(() => test(times("\u6afb", LEN_UCS2), 1, LEN_UCS2, 0))
    // 71 UCS2 chararacters. We'll have |71 - 67 = 4| chararacters in the 2nd
    // segment.
    .then(() => test(times("\u6afb", LEN_UCS2 + 1),
                     2, LEN_UCS2_WITH_8BIT_REF, LEN_UCS2_WITH_8BIT_REF - 4))
    // |LEN_UCS2_WITH_8BIT_REF * 2| ucs2 chararacters.
    .then(() => test(times("\u6afb", LEN_UCS2_WITH_8BIT_REF * 2),
                     2, LEN_UCS2_WITH_8BIT_REF, 0))
    // |LEN_7BIT_WITH_8BIT_REF * 2 + 1| ucs2 chararacters.
    .then(() => test(times("\u6afb", LEN_UCS2_WITH_8BIT_REF * 2 + 1),
                     3, LEN_UCS2_WITH_8BIT_REF, LEN_UCS2_WITH_8BIT_REF - 1))

    // Strict 7-Bit Encoding:
    //
    .then(() => pushPrefEnv({ set: [["dom.sms.strict7BitEncoding", true]] }))

    // Should have no effect on GSM default alphabet characters.
    .then(() => test("\u0041",       1, LEN_7BIT, LEN_7BIT - 1))
    // "\u00c0"(À) should be mapped to "\u0041"(A).
    .then(() => test("\u00c0",       1, LEN_7BIT, LEN_7BIT - 1))
    // Mixing mapped characters with unmapped ones.
    .then(() => test("\u00c0\u0041", 1, LEN_7BIT, LEN_7BIT - 2))
    .then(() => test("\u0041\u00c0", 1, LEN_7BIT, LEN_7BIT - 2))
    // UCS2 characters should be mapped to '*'.
    .then(() => test("\u1234",       1, LEN_7BIT, LEN_7BIT - 1));
});
