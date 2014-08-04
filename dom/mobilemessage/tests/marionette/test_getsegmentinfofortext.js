/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

// Copied from dom/system/gonk/ril_consts.js.
const PDU_MAX_USER_DATA_7BIT = 160;

function test(text, segments, charsPerSegment, charsAvailableInLastSegment) {
  log("Testing '" + text + "' ...");

  let domRequest = manager.getSegmentInfoForText(text);
  ok(domRequest, "DOMRequest object returned.");

  return wrapDomRequestAsPromise(domRequest)
    .then(function(aEvent) {
      let result = aEvent.target.result;
      ok(result, "aEvent.target.result = " + JSON.stringify(result));

      is(result.segments, segments, "result.segments");
      is(result.charsPerSegment, charsPerSegment, "result.charsPerSegment");
      is(result.charsAvailableInLastSegment, charsAvailableInLastSegment,
         "result.charsAvailableInLastSegment");
    });
}

startTestCommon(function() {
  // Ensure we always begin with strict 7bit encoding set to false.
  return pushPrefEnv({ set: [["dom.sms.strict7BitEncoding", false]] })

    .then(() => test(null,      1, PDU_MAX_USER_DATA_7BIT, (PDU_MAX_USER_DATA_7BIT - "null".length)))
    .then(() => test(undefined, 1, PDU_MAX_USER_DATA_7BIT, (PDU_MAX_USER_DATA_7BIT - "undefined".length)))

    .then(() => test(0,         1, PDU_MAX_USER_DATA_7BIT, (PDU_MAX_USER_DATA_7BIT - "0".length)))
    .then(() => test(1.0,       1, PDU_MAX_USER_DATA_7BIT, (PDU_MAX_USER_DATA_7BIT - "1".length)))

    // Testing empty object.  The empty object extends to "[object Object]" and
    // both '[' and ']' are in default single shift table, so each of them
    // takes two septets.
    .then(() => test({},        1, PDU_MAX_USER_DATA_7BIT, (PDU_MAX_USER_DATA_7BIT - (("" + {}).length + 2))))

    .then(function() {
      let date = new Date();
      return test(date,         1, PDU_MAX_USER_DATA_7BIT, (PDU_MAX_USER_DATA_7BIT - ("" + date).length));
    })

    .then(() => test("",        1, PDU_MAX_USER_DATA_7BIT, (PDU_MAX_USER_DATA_7BIT - "".length)));
});
