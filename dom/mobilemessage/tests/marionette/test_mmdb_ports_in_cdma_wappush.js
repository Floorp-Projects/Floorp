/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'mmdb_head.js';

const DBNAME = "test_mmdb_ports_in_cdma_wappush:" + newUUID();

const TEST_PDU = [
  {
    sender: "+0987654321",
    iccId: "1029384756",
    segmentRef: 0,
    segmentSeq: 1, // 1st segment
    segmentMaxSeq: 2,
    encoding: 0x04, // 8-bit encoding
    data: [0, 1, 2],
    teleservice: 0x1004, // PDU_CDMA_MSG_TELESERIVCIE_ID_WAP
    // Port numbers are only provided in 1st segment from CDMA SMS PDUs.
    originatorPort: 9200, // WDP_PORT_PUSH (server-side)
    destinationPort: 2948 // WDP_PORT_PUSH (client-side)
  },
  {
    sender: "+0987654321",
    iccId: "1029384756",
    segmentRef: 0,
    segmentSeq: 2, // 2nd segment
    segmentMaxSeq: 2,
    encoding: 0x04, // 8-bit encoding
    data: [3, 4, 5],
    teleservice: 0x1004, // PDU_CDMA_MSG_TELESERIVCIE_ID_WAP
    // Port numbers are only provided in 1st segment from CDMA SMS PDUs.
    originatorPort: Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID,
    destinationPort: Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID
  }
];

function testSaveCdmaWapPush(aMmdb, aReverse) {
  log("testSaveCdmaWapPush(), aReverse: " + aReverse);

  let testPDUs = aReverse ? Array.from(TEST_PDU).reverse() : TEST_PDU;
  let lengthOfFullData = 0;
  let promises = [];

  for (let pdu of testPDUs) {
    lengthOfFullData += pdu.data.length;
    promises.push(saveSmsSegment(aMmdb, pdu));
  };

  return Promise.all(promises)
    .then((aResults) => {
      // Complete message shall be returned after 2 segments are saved.
      let completeMsg = aResults[1][1];

      is(completeMsg.originatorPort, TEST_PDU[0].originatorPort, "originatorPort");
      is(completeMsg.destinationPort, TEST_PDU[0].destinationPort, "destinationPort");

      is(completeMsg.fullData.length, lengthOfFullData, "fullData.length");
      for (let i = 0; i < lengthOfFullData; i++) {
        is(completeMsg.fullData[i], i, "completeMsg.fullData[" + i + "]");
      }
    });
}

startTestBase(function testCaseMain() {

  let mmdb = newMobileMessageDB();
  return initMobileMessageDB(mmdb, DBNAME, 0)

    .then(() => testSaveCdmaWapPush(mmdb, false))
    .then(() => testSaveCdmaWapPush(mmdb, true))

    .then(() => closeMobileMessageDB(mmdb));
});
