/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// HeapSnapshot.prototype.creationTime returns the expected time.

function waitForThirtyMilliseconds() {
  const start = Date.now();
  while (Date.now() - start < 30) ;
}

function run_test() {
  const start = Date.now() * 1000;
  do_print("start                 = " + start);

  // Because Date.now() is less precise than the snapshot's time stamp, give it
  // a little bit of head room. Additionally, WinXP's timer only has granularity
  // of +/- 15ms.
  waitForThirtyMilliseconds();
  const path = ChromeUtils.saveHeapSnapshot({ runtime: true });
  waitForThirtyMilliseconds();

  const end = Date.now() * 1000;
  do_print("end                   = " + end);

  const snapshot = ChromeUtils.readHeapSnapshot(path);
  do_print("snapshot.creationTime = " + snapshot.creationTime);

  ok(snapshot.creationTime >= start);
  ok(snapshot.creationTime <= end);
}
