/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether the profiler module and actor have the correct state on
 * initialization, activation, and when a clients' connection closes.
 */

const Profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
const MAX_PROFILER_ENTRIES = 10000000;

function run_test()
{
  // Ensure the profiler is not running when the test starts (it could
  // happen if the MOZ_PROFILER_STARTUP environment variable is set).
  Profiler.StopProfiler();

  get_chrome_actors((client1, form1) => {
    let actor1 = form1.profilerActor;
    get_chrome_actors((client2, form2) => {
      let actor2 = form2.profilerActor;
      test_activate(client1, actor1, client2, actor2, () => {
        do_test_finished();
      });
    });
  });

  do_test_pending();
}

function test_activate(client1, actor1, client2, actor2, callback) {
  // Profiler should be inactive at this point.
  client1.request({ to: actor1, type: "isActive" }, response => {
    do_check_false(Profiler.IsActive());
    do_check_false(response.isActive);
    do_check_eq(response.currentTime, undefined);
    do_check_true(typeof response.position === "number");
    do_check_true(typeof response.totalSize === "number");
    do_check_true(typeof response.generation === "number");

    // Start the profiler on the first connection....
    client1.request({ to: actor1, type: "startProfiler", entries: MAX_PROFILER_ENTRIES }, response => {
      do_check_true(Profiler.IsActive());
      do_check_true(response.started);
      do_check_true(typeof response.position === "number");
      do_check_true(typeof response.totalSize === "number");
      do_check_true(typeof response.generation === "number");
      do_check_true(response.position >= 0 && response.position < response.totalSize);
      do_check_true(response.totalSize === MAX_PROFILER_ENTRIES);

      // On the next connection just make sure the actor has been instantiated.
      client2.request({ to: actor2, type: "isActive" }, response => {
        do_check_true(Profiler.IsActive());
        do_check_true(response.isActive);
        do_check_true(response.currentTime > 0);
        do_check_true(typeof response.position === "number");
        do_check_true(typeof response.totalSize === "number");
        do_check_true(typeof response.generation === "number");
        do_check_true(response.position >= 0 && response.position < response.totalSize);
        do_check_true(response.totalSize === MAX_PROFILER_ENTRIES);

        let origConnectionClosed = DebuggerServer._connectionClosed;

        DebuggerServer._connectionClosed = function (conn) {
          origConnectionClosed.call(this, conn);

          // The first client is the only actor that started the profiler,
          // however the second client can request the accumulated profile data
          // at any moment, so the profiler module shouldn't have deactivated.
          do_check_true(Profiler.IsActive());

          DebuggerServer._connectionClosed = function (conn) {
            origConnectionClosed.call(this, conn);

            // Now there are no open clients at all, it should *definitely*
            // be deactivated by now.
            do_check_false(Profiler.IsActive());

            callback();
          };
          client2.close();
        };
        client1.close();
      });
    });
  });
}
