/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'mmdb_head.js';

const DBNAME = "test_mmdb_foreachmatchedmmsdeliveryinfo:" + newUUID();

const PHONE_0 = "+15555215500";
const PHONE_1 = "+15555215501";
const PHONE_2 = "+15555215502";
const PHONE_2_NET = "5555215502";
const PHONE_3 = "+15555215503";
const PHONE_3_NET = "5555215503";
const EMAIL_1 = "foo@bar.com";
var deliveryInfo = [
  { receiver: PHONE_1 },
  { receiver: PHONE_2 },
  { receiver: PHONE_1 },
  { receiver: PHONE_2_NET },
  { receiver: PHONE_3_NET },
  { receiver: EMAIL_1 },
  { receiver: PHONE_3 },
];

function clearTraversed(aDeliveryInfo) {
  for (let element of aDeliveryInfo) {
    delete element.traversed;
  }
}

function doTest(aMmdb, aNeedle, aVerifyFunc, aCount) {
  log("  '" + aNeedle + "': " + aCount);

  clearTraversed(deliveryInfo);

  let count = 0;
  aMmdb.forEachMatchedMmsDeliveryInfo(deliveryInfo, aNeedle, function(aElement) {
    ok(true, "checking " + aElement.receiver);
    ok(!aElement.hasOwnProperty("traversed"), "element.traversed");
    aVerifyFunc(aElement);

    aElement.traversed = true;
    ++count;
  });
  is(count, aCount, "matched count");
}

function testNotFound(aMmdb) {
  log("Testing unavailable");

  doTest(aMmdb, PHONE_0, function(aElement) {
    ok(false, "Should never have a match");
  }, 0);
}

function testDirectMatch(aMmdb) {
  log("Testing direct matching");

  for (let needle of [PHONE_1, EMAIL_1]) {
    let count = deliveryInfo.reduce(function(aCount, aElement) {
      return aElement.receiver == needle ? aCount + 1 : aCount;
    }, 0);
    doTest(aMmdb, needle, function(aElement) {
      is(aElement.receiver, needle, "element.receiver");
    }, count);
  }
}

function testPhoneMatch(aMmdb) {
  log("Testing phone matching");

  let verifyFunc = function(aValid, aElement) {
    ok(aValid.indexOf(aElement.receiver) >= 0, "element.receiver");
  };

  let matchingGroups = [
    [PHONE_2, PHONE_2_NET],
    [PHONE_3, PHONE_3_NET],
  ];
  for (let group of matchingGroups) {
    for (let item of group) {
      doTest(aMmdb, item, verifyFunc.bind(null, group), group.length);
    }
  }
}

startTestBase(function testCaseMain() {
  let mmdb = newMobileMessageDB();
  return initMobileMessageDB(mmdb, DBNAME, 0)
    .then(() => testNotFound(mmdb))
    .then(() => testDirectMatch(mmdb))
    .then(() => testPhoneMatch(mmdb))
    .then(() => closeMobileMessageDB(mmdb));
});
