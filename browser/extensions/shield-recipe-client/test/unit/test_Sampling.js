"use strict";

Cu.import("resource://shield-recipe-client/lib/Sampling.jsm", this);

add_task(async function testStableSample() {
  // Absolute samples
  equal(await Sampling.stableSample("test", 1), true, "stableSample returns true for 100% sample");
  equal(await Sampling.stableSample("test", 0), false, "stableSample returns false for 0% sample");

  // Known samples. The numbers are nonces to make the tests pass
  equal(await Sampling.stableSample("test-0", 0.5), true, "stableSample returns true for known matching sample");
  equal(await Sampling.stableSample("test-1", 0.5), false, "stableSample returns false for known non-matching sample");
});

add_task(async function testBucketSample() {
  // Absolute samples
  equal(await Sampling.bucketSample("test", 0, 10, 10), true, "bucketSample returns true for 100% sample");
  equal(await Sampling.bucketSample("test", 0, 0, 10), false, "bucketSample returns false for 0% sample");

  // Known samples. The numbers are nonces to make the tests pass
  equal(await Sampling.bucketSample("test-0", 0, 5, 10), true, "bucketSample returns true for known matching sample");
  equal(await Sampling.bucketSample("test-1", 0, 5, 10), false, "bucketSample returns false for known non-matching sample");
});

add_task(async function testRatioSample() {
  // Invalid input
  Assert.rejects(Sampling.ratioSample("test", []), "ratioSample rejects for a list with no ratios");

  // Absolute samples
  equal(await Sampling.ratioSample("test", [1]), 0, "ratioSample returns 0 for a list with only 1 ratio");
  equal(
    await Sampling.ratioSample("test", [0, 0, 1, 0]),
    2,
    "ratioSample returns the only non-zero bucket if all other buckets are zero"
  );

  // Known samples. The numbers are nonces to make the tests pass
  equal(await Sampling.ratioSample("test-0", [1, 1]), 0, "ratioSample returns the correct index for known matching sample");
  equal(await Sampling.ratioSample("test-1", [1, 1]), 1, "ratioSample returns the correct index for known non-matching sample");
});

add_task(async function testFractionToKey() {
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

add_task(async function testTruncatedHash() {
  const expected_regex = /[0-9a-f]{12}/;
  const count = 100;
  let successes = 0;
  for (let i = 0; i < count; i++) {
    const h = await Sampling.truncatedHash(Math.random());
    if (expected_regex.test(h)) {
      successes++;
    }
  }
  equal(successes, count, "truncatedHash makes hashes the right length");
});

add_task(async function testBufferToHex() {
  const data = new ArrayBuffer(4);
  const view = new DataView(data);
  view.setUint8(0, 0xff);
  view.setUint8(1, 0x7f);
  view.setUint8(2, 0x3f);
  view.setUint8(3, 0x1f);
  equal(Sampling.bufferToHex(data), "ff7f3f1f");
});
