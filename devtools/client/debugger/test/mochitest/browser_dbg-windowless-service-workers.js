/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that we can detect a new service worker and hit breakpoints that we've
// set in it.
add_task(async function() {
  info("Subtest #1");

  await pushPref("devtools.debugger.features.windowless-service-workers", true);
  await pushPref("devtools.debugger.workers-visible", true);
  await pushPref("dom.serviceWorkers.enabled", true);
  await pushPref("dom.serviceWorkers.testing.enabled", true);

  let dbg = await initDebugger("doc-service-workers.html");

  invokeInTab("registerWorker");

  await waitForSource(dbg, "service-worker.sjs");
  const workerSource = findSource(dbg, "service-worker.sjs");

  await addBreakpoint(dbg, "service-worker.sjs", 13);

  invokeInTab("fetchFromWorker");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 13);

  // Leave the breakpoint and worker in place for the next subtest.
  await resume(dbg);
  await waitForRequestsToSettle(dbg);
  await removeTab(gBrowser.selectedTab);
});

// Test that breakpoints can be immediately hit in service workers when reloading.
add_task(async function() {
  info("Subtest #2");

  const toolbox = await openNewTabAndToolbox(EXAMPLE_URL + "doc-service-workers.html", "jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  await checkWorkerThreads(dbg, 1);

  // The test page will immediately fetch from the service worker if registered.
  await reload(dbg);

  await waitForSource(dbg, "service-worker.sjs");
  const workerSource = findSource(dbg, "service-worker.sjs");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 13);
  await checkWorkerThreads(dbg, 1);

  await resume(dbg);
  await dbg.actions.removeAllBreakpoints(getContext(dbg));

  invokeInTab("unregisterWorker");

  await checkWorkerThreads(dbg, 0);
  await waitForRequestsToSettle(dbg);
  await removeTab(gBrowser.selectedTab);
});

// Test having a waiting and active service worker for the same registration.
add_task(async function() {
  info("Subtest #3");

  const toolbox = await openNewTabAndToolbox(EXAMPLE_URL + "doc-service-workers.html", "jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  invokeInTab("registerWorker");
  await checkWorkerThreads(dbg, 1);
  await checkWorkerStatus(dbg, "activated");

  const firstTab = gBrowser.selectedTab;

  await addTab(EXAMPLE_URL + "service-worker.sjs?setStatus=newServiceWorker");
  await removeTab(gBrowser.selectedTab);

  const secondTab = await addTab(EXAMPLE_URL + "doc-service-workers.html");

  await gBrowser.selectTabAtIndex(gBrowser.tabs.indexOf(firstTab));
  await checkWorkerThreads(dbg, 2);

  const sources = await waitUntilPredicate(() => {
    const list = dbg.selectors.getSourceList().filter(s => s.url.includes("service-worker.sjs"));
    return list.length == 2 ? list : null;
  });
  ok(true, "Found two different sources for service worker");

  await selectSource(dbg, sources[0]);
  await waitForLoadedSource(dbg, sources[0]);
  const content0 = findSourceContent(dbg, sources[0]);

  await selectSource(dbg, sources[1]);
  await waitForLoadedSource(dbg, sources[1]);
  const content1 = findSourceContent(dbg, sources[1]);

  ok(content0.value.includes("newServiceWorker") != content1.value.includes("newServiceWorker"),
     "Got two different sources for service worker");

  // Add a breakpoint for the next subtest.
  await addBreakpoint(dbg, "service-worker.sjs", 2);

  invokeInTab("unregisterWorker");

  await checkWorkerThreads(dbg, 0);
  await waitForRequestsToSettle(dbg);
  await removeTab(firstTab);
  await removeTab(secondTab);

  // Reset the SJS in case we will be repeating the test.
  await addTab(EXAMPLE_URL + "service-worker.sjs?setStatus=");
  await removeTab(gBrowser.selectedTab);
});

// Test setting breakpoints while the service worker is starting up.
add_task(async function() {
  info("Subtest #4");

  const toolbox = await openNewTabAndToolbox(EXAMPLE_URL + "doc-service-workers.html", "jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  invokeInTab("registerWorker");
  await checkWorkerThreads(dbg, 1);

  await waitForSource(dbg, "service-worker.sjs");
  const workerSource = findSource(dbg, "service-worker.sjs");

  await waitForBreakpointCount(dbg, 1);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 2);
  await checkWorkerStatus(dbg, "parsed");

  await addBreakpoint(dbg, "service-worker.sjs", 19);
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 19);
  await checkWorkerStatus(dbg, "installing");

  await addBreakpoint(dbg, "service-worker.sjs", 5);
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 5);
  await checkWorkerStatus(dbg, "activating");

  await resume(dbg);
  invokeInTab("unregisterWorker");

  await checkWorkerThreads(dbg, 0);
  await waitForRequestsToSettle(dbg);
  await removeTab(gBrowser.selectedTab);
});

async function checkWorkerThreads(dbg, count) {
  await waitUntil(() => dbg.selectors.getThreads().length == count);
  ok(true, `Have ${count} threads`);
}

async function checkWorkerStatus(dbg, status) {
  await waitUntil(() => {
    const threads = dbg.selectors.getThreads();
    return threads.some(t => t.serviceWorkerStatus == status);
  });
  ok(true, `Have thread with status ${status}`);
}
