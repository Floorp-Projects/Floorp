/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* globals run_next_test, add_test, ok, equal, Components, XPCOMUtils */
/* exported run_test */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const MANIFEST_URL = "app://system.gaiamobile.org/manifest.webapp";
// secure element access rule format: <secure element name>/<hex string aid>
const MANIFEST = {
  secure_element_access: [
    // rule0, full AID, SIM1 Secure Element
    "SIM1/000102030405060708090A0B0C0D0E",
    // rule1, every AID from embedded Secure Element
    "eSE/*",
    // rule2, every AID which starts with 0xA0, from every Secure Element
    "*/a0*"
  ]
};
const APPID = 1111;
const PAGE_URL = "app://system.gaiamobile.org/index.html";
const TYPE = "nfc-hci-event-transaction";

// dummy messages for testing with additional test property - |expectedResult|
const TEST_MESSAGES = [
  { origin: "SIM1", expectedResult: false },
  { aid: new Uint8Array([0xA0]), expectedResult: true },
  { aid: new Uint8Array([0xDF]), origin: "eSE", expectedResult: true },
  { aid: new Uint8Array([0xA0, 0x01, 0x02, 0x03]), origin: "SIM2", expectedResult: true },
  { aid: new Uint8Array([0x02]), origin: "SIM3", expectedResult: false },
  { aid: new Uint8Array([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                         0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E]),
    origin: "SIM1", expectedResult: true }
];

let HCIEvtTransactionConfigurator = null;
let aceAccessAllowed = true;

function setMockServices(manifest) {
  XPCOMUtils.defineLazyServiceGetter = (obj, service) => {
    if (service === "appsService") {
      obj.appsService = {
        getManifestFor: (manifestURL) => {
          equal(manifestURL, MANIFEST_URL, "should pass proper manifestUrl");
          return Promise.resolve(manifest);
        },
        getAppLocalIdByManifestURL: (manifestURL) => {
          equal(manifestURL, MANIFEST_URL, "should pass proper manifestUrl");
          return APPID;
        },
      };
    }
    else if (service === "aceService") {
      obj.aceService = {
        isAccessAllowed: () => {
          return Promise.resolve(aceAccessAllowed);
        }
      };
    }
  };
}

function getSystemMessageConfigurator() {
  return Cc[
    "@mozilla.org/dom/system-messages/configurator/nfc-hci-event-transaction;1"
  ].createInstance(Ci.nsISystemMessagesConfigurator);
}

function handleRejectedPromise() {
  ok(false, "Promise should not be rejected");
}

function run_test() {
  setMockServices(MANIFEST);
  HCIEvtTransactionConfigurator = getSystemMessageConfigurator();

  ok(!!HCIEvtTransactionConfigurator, "Configurator should be instantiated");
  ok((typeof HCIEvtTransactionConfigurator.shouldDispatch) === "function",
     "shouldDispatch should be a function");

  run_next_test();
}

add_test(function test_shouldDispatch_no_message() {
  HCIEvtTransactionConfigurator
  .shouldDispatch(MANIFEST_URL, PAGE_URL, TYPE, null)
  .then((result) => {
    ok(!result, "Should be false without message");
  })
  .catch(handleRejectedPromise)
  .then(run_next_test);
});

add_test(function test_shouldDispatch_general_rule_validation() {
  let promises = TEST_MESSAGES.map((m) => {
    return HCIEvtTransactionConfigurator
      .shouldDispatch(MANIFEST_URL, PAGE_URL, TYPE, m);
  });

  Promise.all(promises).then((results) => {
    results.forEach((result, idx) => {
      ok(result === TEST_MESSAGES[idx].expectedResult, "tested message: " + JSON.stringify(TEST_MESSAGES[idx]));
    });
  })
  .catch(handleRejectedPromise)
  .then(run_next_test);
});

add_test(function test_shouldDispatch_aceIsAccessAllowed() {
  var testAccessAllowed = () => {
    return HCIEvtTransactionConfigurator
    .shouldDispatch(MANIFEST_URL, PAGE_URL, TYPE, TEST_MESSAGES[1])
    .then((result) => {
      ok(result === aceAccessAllowed, "Should be equal to ACE access decision");
    });
  };

  aceAccessAllowed = false;
  testAccessAllowed().then(() => {
    aceAccessAllowed = true;
    return testAccessAllowed();
  })
  .catch(handleRejectedPromise)
  .then(run_next_test);
});
