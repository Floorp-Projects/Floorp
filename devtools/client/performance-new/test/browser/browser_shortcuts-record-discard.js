/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function test() {
  info(
    "Test that the profiler shortcuts work for start and discarding a profile."
  );
  await setProfilerFrontendUrl(
    "http://example.com/browser/devtools/client/performance-new/test/browser/fake-frontend.html"
  );
  await makeSureProfilerPopupIsEnabled();

  ok(
    profilerButtonIsVisiblyInactive(),
    "The profiler button is not turned on."
  );

  info("Hit CtrlShift+1 to start the profiler");
  EventUtils.synthesizeKey("1", { ctrlKey: true, shiftKey: true });
  await tick();

  // The shortcuts being added is racy for some reason. Spin on a loop until the initial
  // condition is satisified.
  while (true) {
    if (Services.profiler.IsActive()) {
      break;
    }
    info("The profiler shortcut wasn't quite ready yet, try it again.");
    EventUtils.synthesizeKey("1", { ctrlKey: true, shiftKey: true });
    await tick();
  }

  info("Looking to see if the profiler button UI is active.");
  await waitUntil(() => profilerButtonIsVisiblyActive());
  ok(true, "The profiler button is now active.");

  info("Hit CtrlShift+1 to discard to the profiler");
  EventUtils.synthesizeKey("1", { ctrlKey: true, shiftKey: true });

  info("Looking to see if the profiler button UI is inactive.");
  await waitUntil(() => profilerButtonIsVisiblyInactive());
  ok(true, "The profiler button is back to being inactive.");
});
