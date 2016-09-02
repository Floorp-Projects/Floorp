/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the profiler actor returns its buffer status via getBufferInfo.
 */

const Profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
const INITIAL_WAIT_TIME = 100; // ms
const MAX_WAIT_TIME = 20000; // ms
const MAX_PROFILER_ENTRIES = 10000000;

function run_test()
{
  // Ensure the profiler is not running when the test starts (it could
  // happen if the MOZ_PROFILER_STARTUP environment variable is set).
  Profiler.StopProfiler();

  get_chrome_actors((client, form) => {
    let actor = form.profilerActor;
    check_empty_buffer(client, actor, () => {
      activate_profiler(client, actor, startTime => {
        wait_for_samples(client, actor, () => {
          check_buffer(client, actor, () => {
            deactivate_profiler(client, actor, () => {
              client.close().then(do_test_finished);
            });
          });
        });
      });
    });
  });

  do_test_pending();
}

function check_empty_buffer(client, actor, callback)
{
  client.request({ to: actor, type: "getBufferInfo" }, response => {
    do_check_true(response.position === 0);
    do_check_true(response.totalSize === 0);
    do_check_true(response.generation === 0);
    callback();
  });
}

function check_buffer(client, actor, callback)
{
  client.request({ to: actor, type: "getBufferInfo" }, response => {
    do_check_true(typeof response.position === "number");
    do_check_true(typeof response.totalSize === "number");
    do_check_true(typeof response.generation === "number");
    do_check_true(response.position > 0 && response.position < response.totalSize);
    do_check_true(response.totalSize === MAX_PROFILER_ENTRIES);
    // There's no way we'll fill the buffer in this test.
    do_check_true(response.generation === 0);

    callback();
  });
}

function activate_profiler(client, actor, callback)
{
  client.request({ to: actor, type: "startProfiler", entries: MAX_PROFILER_ENTRIES }, response => {
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

function wait_for_samples(client, actor, callback)
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

    client.request({ to: actor, type: "getProfile" }, response => {
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
      callback();
    });
  }

  // Start off with a 100 millisecond delay.
  attempt(INITIAL_WAIT_TIME);
}
