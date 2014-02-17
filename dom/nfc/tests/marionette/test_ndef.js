/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

function testConstructNDEF() {
  try {
    // omit type, id and payload.
    let r = new MozNDEFRecord(0x0);
    is(r.type, null, "r.type should be null")
    is(r.id, null, "r.id should be null")
    is(r.payload, null, "r.payload should be null")

    // omit id and payload.
    r = new MozNDEFRecord(0x0, new Uint8Array());
    is(r.id, null, "r.id should be null")
    is(r.payload, null, "r.payload should be null")

    // omit payload.
    r = new MozNDEFRecord(0x0, new Uint8Array(), new Uint8Array());
    is(r.payload, null, "r.payload should be null")

    ok(true);
  } catch (e) {
    ok(false, 'type, id or payload should be optional. error:' + e);
  }

  runNextTest();
}

let tests = [
  testConstructNDEF
];

SpecialPowers.pushPermissions(
  [{'type': 'settings', 'allow': true, 'context': document}], runTests);
