/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

function testConstructNDEF() {
  try {
    // omit type, id and payload.
    let r = new MozNDEFRecord();
    is(r.tnf, 0, "r.tnf should be 0");
    is(r.type, null, "r.type should be null");
    is(r.id, null, "r.id should be null");
    is(r.payload, null, "r.payload should be null");

    ok(true);
  } catch (e) {
    ok(false, 'type, id or payload should be optional. error:' + e);
  }

  runNextTest();
}

let tests = [
  testConstructNDEF
];

runTests();
