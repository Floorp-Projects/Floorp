/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the event notification service for the profiler actor.
 */

const Profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
const { ProfilerFront } = require("devtools/shared/fronts/profiler");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let [client, form] = yield getChromeActors();
  let front = new ProfilerFront(client, form);

  let events = [0, 0, 0, 0];
  front.on("console-api-profiler", () => events[0]++);
  front.on("profiler-started", () => events[1]++);
  front.on("profiler-stopped", () => events[2]++);
  client.addListener("eventNotification", (type, response) => {
    do_check_true(type === "eventNotification");
    events[3]++;
  });

  yield front.startProfiler();
  yield front.stopProfiler();

  // All should be empty without binding events
  do_check_true(events[0] === 0);
  do_check_true(events[1] === 0);
  do_check_true(events[2] === 0);
  do_check_true(events[3] === 0);

  let ret = yield front.registerEventNotifications({ events: ["console-api-profiler", "profiler-started", "profiler-stopped"] });
  do_check_true(ret.registered.length === 3);

  yield front.startProfiler();
  do_check_true(events[0] === 0);
  do_check_true(events[1] === 1);
  do_check_true(events[2] === 0);
  do_check_true(events[3] === 1, "compatibility events supported for eventNotifications");

  yield front.stopProfiler();
  do_check_true(events[0] === 0);
  do_check_true(events[1] === 1);
  do_check_true(events[2] === 1);
  do_check_true(events[3] === 2, "compatibility events supported for eventNotifications");

  ret = yield front.unregisterEventNotifications({ events: ["console-api-profiler", "profiler-started", "profiler-stopped"] });
  do_check_true(ret.registered.length === 3);
});

function getChromeActors() {
  let deferred = promise.defer();
  get_chrome_actors((client, form) => deferred.resolve([client, form]));
  return deferred.promise;
}
