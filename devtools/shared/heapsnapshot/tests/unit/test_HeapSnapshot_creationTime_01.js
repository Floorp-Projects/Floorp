/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// HeapSnapshot.prototype.creationTime returns the expected time.

function waitForThirtyMilliseconds() {
  const start = Date.now();
  while (Date.now() - start < 30) {
    // do nothing
  }
}

function run_test() {
  const start = Date.now() * 1000;
  info("start                 = " + start);

  // Because Date.now() is less precise than the snapshot's time stamp, give it
  // a little bit of head room. Additionally, WinXP's timer only has granularity
  // of +/- 15ms.
  waitForThirtyMilliseconds();
  const path = ChromeUtils.saveHeapSnapshot({ runtime: true });
  waitForThirtyMilliseconds();

  const end = Date.now() * 1000;
  info("end                   = " + end);

  const snapshot = ChromeUtils.readHeapSnapshot(path);
  info("snapshot.creationTime = " + snapshot.creationTime);

  ok(snapshot.creationTime >= start);
  ok(snapshot.creationTime <= end);
}
