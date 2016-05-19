/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the profiler actor can correctly retrieve a profile after
 * it is activated.
 */

const Profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
const INITIAL_WAIT_TIME = 100; // ms
const MAX_WAIT_TIME = 20000; // ms

function run_test()
{
  get_chrome_actors((client, form) => {
    let actor = form.profilerActor;
    activate_profiler(client, actor, startTime => {
      test_data(client, actor, startTime, () => {
        deactivate_profiler(client, actor, () => {
          client.close(do_test_finished);
        });
      });
    });
  });

  do_test_pending();
}

function activate_profiler(client, actor, callback)
{
  client.request({ to: actor, type: "startProfiler" }, response => {
    do_check_true(response.started);
    client.request({ to: actor, type: "isActive" }, response => {
      do_check_true(response.isActive);
      callback(response.currentTime);
    });
  });
}

function deactivate_profiler(client, actor, callback)
{
  client.request({ to: actor, type: "stopProfiler" }, response => {
    do_check_false(response.started);
    client.request({ to: actor, type: "isActive" }, response => {
      do_check_false(response.isActive);
      callback();
    });
  });
}

function test_data(client, actor, startTime, callback)
{
  function attempt(delay)
  {
    // No idea why, but Components.stack.sourceLine returns null.
    let funcLine = Components.stack.lineNumber - 3;

    // Spin for the requested time, then take a sample.
    let start = Date.now();
    let stack;
    do_print("Attempt: delay = " + delay);
    while (Date.now() - start < delay) { stack = Components.stack; }
    do_print("Attempt: finished waiting.");

    client.request({ to: actor, type: "getProfile", startTime }, response => {
      // Any valid getProfile response should have the following top
      // level structure.
      do_check_eq(typeof response.profile, "object");
      do_check_eq(typeof response.profile.meta, "object");
      do_check_eq(typeof response.profile.meta.platform, "string");
      do_check_eq(typeof response.profile.threads, "object");
      do_check_eq(typeof response.profile.threads[0], "object");
      do_check_eq(typeof response.profile.threads[0].samples, "object");

      // At this point, we may or may not have samples, depending on
      // whether the spin loop above has given the profiler enough time
      // to get started.
      if (response.profile.threads[0].samples.length == 0) {
        if (delay < MAX_WAIT_TIME) {
          // Double the spin-wait time and try again.
          do_print("Attempt: no samples, going around again.");
          return attempt(delay * 2);
        } else {
          // We've waited long enough, so just fail.
          do_print("Attempt: waited a long time, but no samples were collected.");
          do_print("Giving up.");
          do_check_true(false);
          return;
        }
      }

      // Now check the samples. At least one sample is expected to
      // have been in the busy wait above.
      let loc = stack.name + " (" + stack.filename + ":" + funcLine + ")";
      let thread0 = response.profile.threads[0];
      do_check_true(thread0.samples.data.some(sample => {
        let frames = getInflatedStackLocations(thread0, sample);
        return frames.length != 0 &&
               frames.some(location => (location == loc));
      }));

      callback();
    });
  }

  // Start off with a 100 millisecond delay.
  attempt(INITIAL_WAIT_TIME);
}
