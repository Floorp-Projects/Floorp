/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "head.js";

let url = "https://www.example.com";

function compareNDEFs(ndef1, ndef2) {
  is(ndef1.length, ndef2.length,
     "NDEF messages have the same number of records");
  ndef1.forEach(function(record1, index) {
    let record2 = this[index];
    is(record1.tnf, record2.tnf, "test for equal TNF fields");
    let fields = ["type", "id", "payload"];
    fields.forEach(function(value) {
        let field1 = record1[value];
        let field2 = record2[value];
        is(field1.length, field2.length,
           value + " fields have the same length");
        let eq = true;
        for (let i = 0; eq && i < field1.length; ++i) {
          eq = (field1[i] === field2[i]);
        }
        ok(eq, value + " fields contain the same data");
      });
  }, ndef2);
}

function parseNDEFString(str) {
  /* make it an object */
  let arr = null;
  try {
    arr = JSON.parse(str);
  } catch (e) {
    ok(false, "Parser error: " + e.message);
    return null;
  }
  /* and build NDEF array */
  let ndef = arr.map(function(value) {
      let type = new Uint8Array(NfcUtils.fromUTF8(this.atob(value.type)));
      let id = new Uint8Array(NfcUtils.fromUTF8(this.atob(value.id)));
      let payload = new Uint8Array(NfcUtils.fromUTF8(this.atob(value.payload)));
      return new MozNDEFRecord(value.tnf, type, id, payload);
    }, window);
  return ndef;
}

function sendNDEF(techType, sessionToken) {
  let tnf = NDEF.TNF_WELL_KNOWN;
  let type = new Uint8Array(NfcUtils.fromUTF8("U"));
  let id = new Uint8Array(NfcUtils.fromUTF8(""));
  let payload = new Uint8Array(NfcUtils.fromUTF8(url));
  let ndef = [new MozNDEFRecord(tnf, type, id, payload)];

  let peer = window.navigator.mozNfc.getNFCPeer(sessionToken);
  let req = peer.sendNDEF(ndef);
  req.onsuccess = function() {
    log("Successfully sent NDEF message");

    let cmd = "nfc snep put -1 -1"; /* read last SNEP PUT from emulator */
    log("Executing \'" + cmd + "\'");
    emulator.run(cmd, function(result) {
      is(result.pop(), "OK", "check SNEP PUT result");
      let ndef2 = parseNDEFString(result.pop());
      compareNDEFs(ndef, ndef2);
      toggleNFC(false, runNextTest);
    });
  };
  req.onerror = function() {
    ok(false, "Failed to send NDEF message, error \'" + this.error + "\'");
    toggleNFC(false, runNextTest);
  };
}

function handleTechnologyDiscoveredRE0(msg) {
  log("Received \'nfc-manager-tech-discovered\' " + JSON.stringify(msg));
  is(msg.type, "techDiscovered", "check for correct message type");
  let index = msg.techList.indexOf("P2P");
  isnot(index, -1, "check for \'P2P\' in tech list");
  sendNDEF(msg.techList[index], msg.sessionToken);
}

function activateRE0() {
  let cmd = "nfc nci rf_intf_activated_ntf 0";
  log("Executing \'" + cmd + "\'");
  emulator.run(cmd, function(result) {
    is(result.pop(), "OK", "check activation of RE0");
  });
}

function testOnPeerReadyRE0() {
  log("Running \'testOnPeerReadyRE0\'");
  window.navigator.mozSetMessageHandler(
    "nfc-manager-tech-discovered", handleTechnologyDiscoveredRE0);
  toggleNFC(true, activateRE0);
}

let tests = [
  testOnPeerReadyRE0
];

SpecialPowers.pushPermissions(
  [{"type": "nfc", "allow": true,
                   "read": true, 'write': true, context: document},
   {"type": "nfc-manager", 'allow': true, context: document}], runTests);
