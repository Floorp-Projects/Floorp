"use strict";
// Cu is defined in xpc_head.js
/* globals Cu */

Cu.import("resource://shield-recipe-client/lib/Sampling.jsm", this);

add_task(function* testStableSample() {
  // Absolute samples
  equal(yield Sampling.stableSample("test", 1), true, "stableSample returns true for 100% sample");
  equal(yield Sampling.stableSample("test", 0), false, "stableSample returns false for 0% sample");

  // Known samples. The numbers are nonces to make the tests pass
  equal(yield Sampling.stableSample("test-0", 0.5), true, "stableSample returns true for known matching sample");
  equal(yield Sampling.stableSample("test-1", 0.5), false, "stableSample returns false for known non-matching sample");
});

add_task(function* testBucketSample() {
  // Absolute samples
  equal(yield Sampling.bucketSample("test", 0, 10, 10), true, "bucketSample returns true for 100% sample");
  equal(yield Sampling.bucketSample("test", 0, 0, 10), false, "bucketSample returns false for 0% sample");

  // Known samples. The numbers are nonces to make the tests pass
  equal(yield Sampling.bucketSample("test-0", 0, 5, 10), true, "bucketSample returns true for known matching sample");
  equal(yield Sampling.bucketSample("test-1", 0, 5, 10), false, "bucketSample returns false for known non-matching sample");
});

add_task(function* testFractionToKey() {
  // Test that results are always 12 character hexadecimal strings.
  const expected_regex = /[0-9a-f]{12}/;
  const count = 100;
  let successes = 0;
  for (let i = 0; i < count; i++) {
    const p = Sampling.fractionToKey(Math.random());
    if (expected_regex.test(p)) {
      successes++;
    }
  }
  equal(successes, count, "fractionToKey makes keys the right length");
});

add_task(function* testTruncatedHash() {
  const expected_regex = /[0-9a-f]{12}/;
  const count = 100;
  let successes = 0;
  for (let i = 0; i < count; i++) {
    const h = yield Sampling.truncatedHash(Math.random());
    if (expected_regex.test(h)) {
      successes++;
    }
  }
  equal(successes, count, "truncatedHash makes hashes the right length");
});

add_task(function* testBufferToHex() {
  const data = new ArrayBuffer(4);
  const view = new DataView(data);
  view.setUint8(0, 0xff);
  view.setUint8(1, 0x7f);
  view.setUint8(2, 0x3f);
  view.setUint8(3, 0x1f);
  equal(Sampling.bufferToHex(data), "ff7f3f1f");
});
