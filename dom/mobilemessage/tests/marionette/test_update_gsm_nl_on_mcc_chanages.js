/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const kPrefLastKnownSimMcc = "ril.lastKnownSimMcc";

// "tablespoon" in Turkish. The character 'ç', 'ş' and 'ğ' exist in both Turkish
// locking and single shift tables, but is not available in either GSM default
// alphabet or its extension table.
const MSG_TURKISH = "çorba kaşığı";

// Random sentence taken from Turkish news with 155 characters. It's the longest
// message which can be encoded as single 7-bit SMS segment (while using UCS-2
// it will be splitted to 3 segments).
const MSG_TURKISH_LONG = "Sinan Akçıl-Ebru Şallı cephesinde sular durulmuyor. \
Çiftin ayrılığına dair yeni iddialar ortaya atıldı. İlk iddiaya göre; Sinan \
Akçıl, Ebru Şallı'dan 1.5..";

// Random sentence taken from Turkish news with 156 characters, which just
// exceeds the max length of single 7-bit SMS segment.
const MSG_TURKISH_MULTI_SEGS = "ABD'de Başkan Barack Obama'nın kendisini \
Twitter'dan takip ettiğini söyleyen Kam Brock adlı kadının psikiyatri tedavisi \
görmeye zorlandığı bildirildi. Obama";

function getSegmentInfoForText(aBody) {
  return new Promise(function(resolve, reject) {
    let domRequest = manager.getSegmentInfoForText(aBody);
    domRequest.onsuccess = function() {
      let segmentInfo = domRequest.result;
      log("getSegmentInfoForText success: " + JSON.stringify(segmentInfo));
      resolve(segmentInfo);
    };
    domRequest.onerror = function(){
      log("getSegmentInfoForText error");
      reject();
    }
  });
}

function verifySegmentInfo(segmentInfo, expectedSegmentInfo, msg) {
  is(segmentInfo.segments, expectedSegmentInfo.segments, msg);
  is(segmentInfo.charsPerSegment, expectedSegmentInfo.charsPerSegment, msg);
  is(segmentInfo.charsAvailableInLastSegment,
    expectedSegmentInfo.charsAvailableInLastSegment, msg);
};

/**
 * Since charsPerSegment returned by getSegmentInfoForText should match the
 * actual segment size when sending a message (which is verified in
 * test_ril_worker_sms_sgement_info.js), here we verify the correct table tuples
 * are loaded when MCC changes by comparing the value of charsPerSegment for the
 * same text when applying different mcc values.
 */
startTestCommon(function testCaseMain() {
  return Promise.resolve()
    // Change MCC to US.
    .then(() => pushPrefEnv({set: [[kPrefLastKnownSimMcc, "310"]]}))

    // US / UCS-2 / short Turkish message.
    .then(() => getSegmentInfoForText(MSG_TURKISH))
    .then((segmentInfo) => verifySegmentInfo(segmentInfo,
      {segments: 1, charsPerSegment: 70,
        charsAvailableInLastSegment: 70 - MSG_TURKISH.length},
      "US / UCS-2 / short Turkish message."))

    // US / UCS-2 / long Turkish message.
    .then(() => getSegmentInfoForText(MSG_TURKISH_LONG))
    .then((segmentInfo) => verifySegmentInfo(segmentInfo,
      {segments: 3, charsPerSegment: 67,
        charsAvailableInLastSegment: 67 - MSG_TURKISH_LONG.length % 67},
      "US / UCS-2 / long Turkish message."))

    // Change MCC to Turkey.
    .then(() => pushPrefEnv({set: [[kPrefLastKnownSimMcc, "286"]]}))

    // Turkey / GSM 7 bits / short Turkish message.
    .then(() => getSegmentInfoForText(MSG_TURKISH))
    .then((segmentInfo) => verifySegmentInfo(segmentInfo,
      {segments: 1, charsPerSegment: 155,
        charsAvailableInLastSegment: 155 - MSG_TURKISH.length},
      "Turkey / GSM 7 bits / short Turkish message."))

    // Turkey; GSM 7 bits; longest single segment Turkish message.
    .then(() => getSegmentInfoForText(MSG_TURKISH_LONG))
    .then((segmentInfo) => verifySegmentInfo(segmentInfo,
      {segments: 1, charsPerSegment: 155, charsAvailableInLastSegment: 0},
      "Turkey / GSM 7 bits / longest single segment Turkish message."))

    // Turkey; GSM 7 bits; shortest dual segments Turkish message.
    .then(() => getSegmentInfoForText(MSG_TURKISH_MULTI_SEGS))
    .then((segmentInfo) => verifySegmentInfo(segmentInfo,
      {segments: 2, charsPerSegment: 149,
        charsAvailableInLastSegment: 149 - MSG_TURKISH_MULTI_SEGS.length % 149},
      "Turkey / GSM 7 bits / shortest dual segments Turkish message."));
});
