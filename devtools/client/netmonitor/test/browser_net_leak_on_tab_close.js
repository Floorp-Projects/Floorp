/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that netmonitor doesn't leak windows on parent-side pages (bug 1285638)
 */

add_task(function* () {
  // Tell initNetMonitor to enable cache. Otherwise it will assert that there were more
  // than zero network requests during the page load. But when loading about:config,
  // there are none.
  let { monitor } = yield initNetMonitor("about:config", null, true);
  ok(monitor, "The network monitor was opened");
  yield teardown(monitor);
});
