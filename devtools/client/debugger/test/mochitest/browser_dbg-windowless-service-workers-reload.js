/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that service worker sources are still displayed after reloading the page
// and that we can hit breakpoints in them.

"use strict";

add_task(async function () {
  await pushPref("devtools.debugger.features.windowless-service-workers", true);
  await pushPref("devtools.debugger.threads-visible", true);
  await pushPref("dom.serviceWorkers.enabled", true);
  await pushPref("dom.serviceWorkers.testing.enabled", true);

  const dbg = await initDebugger("doc-service-workers.html");

  invokeInTab("registerWorker");
  await waitForSource(dbg, "service-worker.sjs");
  const workerSource = findSource(dbg, "service-worker.sjs");

  await reload(dbg, "service-worker.sjs");

  await addBreakpoint(dbg, "service-worker.sjs", 13);
  invokeInTab("fetchFromWorker");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 13);

  await assertPreviews(dbg, [
    {
      line: 10,
      column: 9,
      result: EXAMPLE_URL + "whatever",
      expression: "url",
    },
  ]);

  await resume(dbg);

  // Reload a second time to ensure we can still debug the SW
  await reload(dbg, "service-worker.sjs");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 13);

  await assertPreviews(dbg, [
    {
      line: 10,
      column: 9,
      result: EXAMPLE_URL + "whatever",
      expression: "url",
    },
  ]);

  await resume(dbg);

  // Help the SW to be immediately destroyed after unregistering it.
  // Do not use too low value as it needs to keep running while we do assertions against debugger UI.
  // This can be slow on debug builds.
  await pushPref("dom.serviceWorkers.idle_timeout", 500);
  await pushPref("dom.serviceWorkers.idle_extended_timeout", 500);

  const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );
  // Unfortunately, swm.getRegistrationByPrincipal doesn't work and throw,
  // so go over the live list of all SW to find the matching SW info,
  // in order to retrieve its active worker,
  // in order to call attach+detachDebugger,
  // in order to reset the idle timeout.
  const registrations = swm.getAllRegistrations();
  for (let i = 0; i < registrations.length; i++) {
    const info = registrations.queryElementAt(
      i,
      Ci.nsIServiceWorkerRegistrationInfo
    );
    if (info.scriptSpec.endsWith("service-worker.sjs")) {
      info.activeWorker.attachDebugger();
      info.activeWorker.detachDebugger();
    }
  }

  // Thanks to previous hack, the following call should immediately unregister the SW
  invokeInTab("unregisterWorker");

  await checkAdditionalThreadCount(dbg, 0);

  await waitForRequestsToSettle(dbg);
  await removeTab(gBrowser.selectedTab);
});
