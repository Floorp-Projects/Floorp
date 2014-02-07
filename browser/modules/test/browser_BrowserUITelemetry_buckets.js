/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
    WHERE'S MAH BUCKET?!
          \
                     ___
                  .-9 9 `\
                =(:(::)=  ;
                  ||||     \
                  ||||      `-.
                 ,\|\|         `,
                /                \
               ;                  `'---.,
               |                         `\
               ;                     /     |
               \                    |      /
                )           \  __,.--\    /
             .-' \,..._\     \`   .-'  .-'
            `-=``      `:    |  /-/-/`
                         `.__/
*/

"use strict";


function generatorTest() {
  let s = {};
  Components.utils.import("resource:///modules/BrowserUITelemetry.jsm", s);
  let BUIT = s.BrowserUITelemetry;

  registerCleanupFunction(function() {
    BUIT.setBucket(null);
  });


  // setBucket
  is(BUIT.currentBucket, BUIT.BUCKET_DEFAULT, "Bucket should be default bucket");
  BUIT.setBucket("mah-bucket");
  is(BUIT.currentBucket, BUIT.BUCKET_PREFIX + "mah-bucket", "Bucket should have correct name");
  BUIT.setBucket(null);
  is(BUIT.currentBucket, BUIT.BUCKET_DEFAULT, "Bucket should be reset to default");


  // _toTimeStr
  is(BUIT._toTimeStr(10), "10ms", "Checking time string reprentation, 10ms");
  is(BUIT._toTimeStr(1000 + 10), "1s10ms", "Checking time string reprentation, 1s10ms");
  is(BUIT._toTimeStr((20 * 1000) + 10), "20s10ms", "Checking time string reprentation, 20s10ms");
  is(BUIT._toTimeStr(60 * 1000), "1m", "Checking time string reprentation, 1m");
  is(BUIT._toTimeStr(3 * 60 * 1000), "3m", "Checking time string reprentation, 3m");
  is(BUIT._toTimeStr((3 * 60 * 1000) + 1), "3m1ms", "Checking time string reprentation, 3m1ms");
  is(BUIT._toTimeStr((60 * 60 * 1000) + (10 * 60 * 1000)), "1h10m", "Checking time string reprentation, 1h10m");
  is(BUIT._toTimeStr(100 * 60 * 60 * 1000), "100h", "Checking time string reprentation, 100h");


  // setExpiringBucket
  BUIT.setExpiringBucket("walrus", [1001, 2001, 3001, 10001]);
  is(BUIT.currentBucket, BUIT.BUCKET_PREFIX + "walrus" + BUIT.BUCKET_SEPARATOR + "1s1ms",
     "Bucket should be expiring and have time step of 1s1ms");

  waitForCondition(function() {
    return BUIT.currentBucket == (BUIT.BUCKET_PREFIX + "walrus" + BUIT.BUCKET_SEPARATOR + "2s1ms");
  }, nextStep, "Bucket should be expiring and have time step of 2s1ms");
  yield undefined;

  waitForCondition(function() {
    return BUIT.currentBucket == (BUIT.BUCKET_PREFIX + "walrus" + BUIT.BUCKET_SEPARATOR + "3s1ms");
  }, nextStep, "Bucket should be expiring and have time step of 3s1ms");
  yield undefined;


  // Interupt previous expiring bucket
  BUIT.setExpiringBucket("walrus2", [1002, 2002]);
  is(BUIT.currentBucket, BUIT.BUCKET_PREFIX + "walrus2" + BUIT.BUCKET_SEPARATOR + "1s2ms",
     "Should be new expiring bucket, with time step of 1s2ms");

  waitForCondition(function() {
    return BUIT.currentBucket == (BUIT.BUCKET_PREFIX + "walrus2" + BUIT.BUCKET_SEPARATOR + "2s2ms");
  }, nextStep, "Should be new expiring bucket, with time step of 2s2ms");
  yield undefined;


  // Let expiring bucket expire
  waitForCondition(function() {
    return BUIT.currentBucket == BUIT.BUCKET_DEFAULT;
  }, nextStep, "Bucket should have expired, default bucket should now be active");
  yield undefined;


  // Interupt expiring bucket with normal bucket
  BUIT.setExpiringBucket("walrus3", [1003, 2003]);
  is(BUIT.currentBucket, BUIT.BUCKET_PREFIX + "walrus3" + BUIT.BUCKET_SEPARATOR + "1s3ms",
     "Should be new expiring bucket, with time step of 1s3ms");

  BUIT.setBucket("mah-bucket");
  is(BUIT.currentBucket, BUIT.BUCKET_PREFIX + "mah-bucket", "Bucket should have correct name");

  waitForCondition(function() {
    return BUIT.currentBucket == (BUIT.BUCKET_PREFIX + "mah-bucket");
  }, nextStep, "Next step of old expiring bucket shouldn't have progressed");
  yield undefined;
}
