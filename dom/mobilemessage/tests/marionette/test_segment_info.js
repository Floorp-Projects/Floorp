/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const LEN_7BIT = 160;
const LEN_7BIT_WITH_8BIT_REF = 153;
const LEN_7BIT_WITH_16BIT_REF = 152;
const LEN_UCS2 = 70;
const LEN_UCS2_WITH_8BIT_REF = 67;
const LEN_UCS2_WITH_16BIT_REF = 66;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
let currentStrict7BitEncoding = false;
SpecialPowers.setBoolPref("dom.sms.strict7BitEncoding",
                          currentStrict7BitEncoding);
SpecialPowers.addPermission("sms", true, document);

let manager = window.navigator.mozMobileMessage;
ok(manager instanceof MozMobileMessageManager,
   "manager is instance of " + manager.constructor);

function times(str, n) {
  return (new Array(n + 1)).join(str);
}

let tasks = {
  // List of test fuctions. Each of them should call |tasks.next()| when
  // completed or |tasks.finish()| to jump to the last one.
  _tasks: [],
  _nextTaskIndex: 0,

  push: function(func) {
    this._tasks.push(func);
  },

  next: function() {
    let index = this._nextTaskIndex++;
    let task = this._tasks[index];
    try {
      task();
    } catch (ex) {
      ok(false, "test task[" + index + "] throws: " + ex);
      // Run last task as clean up if possible.
      if (index != this._tasks.length - 1) {
        this.finish();
      }
    }
  },

  finish: function() {
    this._tasks[this._tasks.length - 1]();
  },

  run: function() {
    this.next();
  }
};

function addTest(text, strict7BitEncoding, expected) {
  tasks.push(function() {
    if (strict7BitEncoding != currentStrict7BitEncoding) {
      currentStrict7BitEncoding = strict7BitEncoding;
      SpecialPowers.setBoolPref("dom.sms.strict7BitEncoding",
                                currentStrict7BitEncoding);
    }

    let domRequest = manager.getSegmentInfoForText(text);
    ok(domRequest, "DOMRequest object returned.");

    domRequest.onsuccess = function(e) {
      log("Received 'onsuccess' DOMRequest event.");

      let result = e.target.result;
      if (!result) {
        ok(false, "getSegmentInfoForText() result is not valid.");
        tasks.finish();
        return;
      }

      is(result.segments, expected[0], "segments");
      is(result.charsPerSegment, expected[1], "charsPerSegment");
      is(result.charsAvailableInLastSegment, expected[2],
         "charsAvailableInLastSegment");

      tasks.next();
    };

    domRequest.onerror = function(e) {
      ok(false, "Failed to call getSegmentInfoForText().");
      tasks.finish();
    };
  });
}

// GSM 7Bit Alphabets:
//
// 'a' is in GSM default locking shift table, so it takes 1 septet.
addTest("a", false, [1, LEN_7BIT, LEN_7BIT - 1]);
// '\u20ac' is in GSM default single shift table, so it takes 2 septets.
addTest("\u20ac", false, [1, LEN_7BIT, LEN_7BIT - 2]);
// SP is defined in both locking shift and single shift tables.
addTest(" ", false, [1, LEN_7BIT, LEN_7BIT - 1]);
// Some combinations.
addTest("a\u20ac", false, [1, LEN_7BIT, LEN_7BIT - 3]);
addTest("a ", false, [1, LEN_7BIT, LEN_7BIT - 2]);
addTest("\u20aca", false, [1, LEN_7BIT, LEN_7BIT - 3]);
addTest("\u20ac ", false, [1, LEN_7BIT, LEN_7BIT - 3]);
addTest(" \u20ac", false, [1, LEN_7BIT, LEN_7BIT - 3]);
addTest(" a", false, [1, LEN_7BIT, LEN_7BIT - 2]);

// GSM 7Bit Alphabets (multipart):
//
// Exactly 160 locking shift table chararacters.
addTest(times("a", LEN_7BIT), false, [1, LEN_7BIT, 0]);
// 161 locking shift table chararacters. We'll have |161 - 153 = 8| septets in
// the 2nd segment.
addTest(times("a", LEN_7BIT + 1), false,
        [2, LEN_7BIT_WITH_8BIT_REF, LEN_7BIT_WITH_8BIT_REF - 8]);
// |LEN_7BIT_WITH_8BIT_REF * 2| locking shift table chararacters.
addTest(times("a", LEN_7BIT_WITH_8BIT_REF * 2), false,
        [2, LEN_7BIT_WITH_8BIT_REF, 0]);
// |LEN_7BIT_WITH_8BIT_REF * 2 + 1| locking shift table chararacters.
addTest(times("a", LEN_7BIT_WITH_8BIT_REF * 2 + 1), false,
        [3, LEN_7BIT_WITH_8BIT_REF, LEN_7BIT_WITH_8BIT_REF - 1]);
// Exactly 80 single shift table chararacters.
addTest(times("\u20ac", LEN_7BIT / 2), false, [1, LEN_7BIT, 0]);
// 81 single shift table chararacters. Because |Math.floor(153 / 2) = 76|, it
// should left 5 septets in the 2nd segment.
addTest(times("\u20ac", LEN_7BIT / 2 + 1), false,
        [2, LEN_7BIT_WITH_8BIT_REF, LEN_7BIT_WITH_8BIT_REF - 10]);
// |1 + 2 * 76| single shift table chararacters. We have only |153 - 76 * 2 = 1|
// space left, but each single shift table character takes 2, so it will be
// filled in the 3rd segment.
addTest(times("\u20ac", 1 + 2 * Math.floor(LEN_7BIT_WITH_8BIT_REF / 2)), false,
        [3, LEN_7BIT_WITH_8BIT_REF, LEN_7BIT_WITH_8BIT_REF - 2]);
// |2 * 76| single shift table chararacters + 1 locking shift table chararacter.
addTest("a" + times("\u20ac", 2 * Math.floor(LEN_7BIT_WITH_8BIT_REF / 2)), false,
        [2, LEN_7BIT_WITH_8BIT_REF, 1]);
addTest(times("\u20ac", 2 * Math.floor(LEN_7BIT_WITH_8BIT_REF / 2)) + "a", false,
        [2, LEN_7BIT_WITH_8BIT_REF, 0]);

// UCS2:
//
// '\u6afb' should be encoded as UCS2.
addTest("\u6afb", false, [1, LEN_UCS2, LEN_UCS2 - 1]);
// Combination of GSM 7bit alphabets.
addTest("\u6afba", false, [1, LEN_UCS2, LEN_UCS2 - 2]);
addTest("\u6afb\u20ac", false, [1, LEN_UCS2, LEN_UCS2 - 2]);
addTest("\u6afb ", false, [1, LEN_UCS2, LEN_UCS2 - 2]);

// UCS2 (multipart):
//
// Exactly 70 UCS2 chararacters.
addTest(times("\u6afb", LEN_UCS2), false, [1, LEN_UCS2, 0]);
// 71 UCS2 chararacters. We'll have |71 - 67 = 4| chararacters in the 2nd
// segment.
addTest(times("\u6afb", LEN_UCS2 + 1), false,
        [2, LEN_UCS2_WITH_8BIT_REF, LEN_UCS2_WITH_8BIT_REF - 4]);
// |LEN_UCS2_WITH_8BIT_REF * 2| ucs2 chararacters.
addTest(times("\u6afb", LEN_UCS2_WITH_8BIT_REF * 2), false,
        [2, LEN_UCS2_WITH_8BIT_REF, 0]);
// |LEN_7BIT_WITH_8BIT_REF * 2 + 1| ucs2 chararacters.
addTest(times("\u6afb", LEN_UCS2_WITH_8BIT_REF * 2 + 1), false,
        [3, LEN_UCS2_WITH_8BIT_REF, LEN_UCS2_WITH_8BIT_REF - 1]);

// Strict 7-Bit Encoding:
//
// Should have no effect on GSM default alphabet characters.
addTest("\u0041", true, [1, LEN_7BIT, LEN_7BIT - 1]);
// "\u00c0"(À) should be mapped to "\u0041"(A).
addTest("\u00c0", true, [1, LEN_7BIT, LEN_7BIT - 1]);
// Mixing mapped characters with unmapped ones.
addTest("\u00c0\u0041", true, [1, LEN_7BIT, LEN_7BIT - 2]);
addTest("\u0041\u00c0", true, [1, LEN_7BIT, LEN_7BIT - 2]);
// UCS2 characters should be mapped to '*'.
addTest("\u1234", true, [1, LEN_7BIT, LEN_7BIT - 1]);


// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  SpecialPowers.clearUserPref("dom.sms.strict7BitEncoding");
  finish();
});

tasks.run();
