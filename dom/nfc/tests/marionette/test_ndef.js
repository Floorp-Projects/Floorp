/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = 'head.js';

function testConstructNDEF() {
  try {
    // omit type, id and payload.
    let r = new MozNDEFRecord();
    is(r.tnf, "empty", "r.tnf should be 'empty'");
    is(r.type, null, "r.type should be null");
    is(r.id, null, "r.id should be null");
    is(r.payload, null, "r.payload should be null");

    ok(true);
  } catch (e) {
    ok(false, 'type, id or payload should be optional. error:' + e);
  }

  try {
    new MozNDEFRecord({type: new Uint8Array(1)});
    ok(false, "new MozNDEFRecord should fail, type should be null for empty tnf");
  } catch (e){
    ok(true);
  }

  try {
    new MozNDEFRecord({tnf: "unknown", type: new Uint8Array(1)});
    ok(false, "new MozNDEFRecord should fail, type should be null for unknown tnf");
  } catch (e){
    ok(true);
  }

  try {
    new MozNDEFRecord({tnf: "unchanged", type: new Uint8Array(1)});
    ok(false, "new MozNDEFRecord should fail, type should be null for unchanged tnf");
  } catch (e){
    ok(true);
  }

  try {
    new MozNDEFRecord({tnf: "illegal", type: new Uint8Array(1)});
    ok(false, "new MozNDEFRecord should fail, invalid tnf");
  } catch (e){
    ok(true);
  }

  runNextTest();
}

var tests = [
  testConstructNDEF
];

runTests();
