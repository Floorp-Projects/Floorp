/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the event notification service for the profiler actor.
 */

const Profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
const MAX_PROFILER_ENTRIES = 10000000;
const { ProfilerFront } = require("devtools/server/actors/profiler");
const { waitForTime } = DevToolsUtils;

function run_test() {
  run_next_test();
}

add_task(function *() {
  let [client, form] = yield getChromeActors();
  let front = new ProfilerFront(client, form);

  // Ensure the profiler is not running when the test starts (it could
  // happen if the MOZ_PROFILER_STARTUP environment variable is set).
  Profiler.StopProfiler();
  let eventsCalled = 0;
  let handledThreeTimes = promise.defer();

  front.on("profiler-status", (response) => {
    dump("'profiler-status' fired\n");
    do_check_true(typeof response.position === "number");
    do_check_true(typeof response.totalSize === "number");
    do_check_true(typeof response.generation === "number");
    do_check_true(response.position > 0 && response.position < response.totalSize);
    do_check_true(response.totalSize === MAX_PROFILER_ENTRIES);
    // There's no way we'll fill the buffer in this test.
    do_check_true(response.generation === 0);

    eventsCalled++;
    if (eventsCalled > 2) {
      handledThreeTimes.resolve();
    }
  });

  yield front.setProfilerStatusInterval(1);
  dump("Set the profiler-status event interval to 1\n");
  yield front.startProfiler();
  yield waitForTime(500);
  yield front.stopProfiler();

  do_check_true(eventsCalled === 0, "No 'profiler-status' events should be fired before registering.");

  let ret = yield front.registerEventNotifications({ events: ["profiler-status"] });
  do_check_true(ret.registered.length === 1);

  yield front.startProfiler();
  yield handledThreeTimes.promise;
  yield front.stopProfiler();
  do_check_true(eventsCalled >= 3, "profiler-status fired atleast three times while recording");

  let totalEvents = eventsCalled;
  yield waitForTime(50);
  do_check_true(totalEvents === eventsCalled, "No more profiler-status events after recording.");
});

function getChromeActors () {
  let deferred = promise.defer();
  get_chrome_actors((client, form) => deferred.resolve([client, form]));
  return deferred.promise;
}
