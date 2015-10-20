/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// HeapSnapshot.prototype.creationTime returns the expected time.

function waitForTenMilliseconds() {
  const start = Date.now();
  while (Date.now() - start < 10) ;
}

function run_test() {
  const start = Date.now() * 1000;
  do_print("start                 = " + start);

  // Because Date.now() is less precise than the snapshot's time stamp, give it
  // a little bit of head room.
  waitForTenMilliseconds();
  const path = ChromeUtils.saveHeapSnapshot({ runtime: true });
  waitForTenMilliseconds();

  const end = Date.now() * 1000;
  do_print("end                   = " + end);

  const snapshot = ChromeUtils.readHeapSnapshot(path);
  do_print("snapshot.creationTime = " + snapshot.creationTime);

  ok(snapshot.creationTime >= start);
  ok(snapshot.creationTime <= end);
}
