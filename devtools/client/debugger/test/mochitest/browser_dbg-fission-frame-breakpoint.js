/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const TEST_COM_URI = `${URL_ROOT_COM_SSL}examples/doc_dbg-fission-frame-sources.html`;

add_task(async function () {
  // Load a test page with a remote frame:
  // simple1.js is imported by the main page. simple2.js comes from the remote frame.
  const dbg = await initDebuggerWithAbsoluteURL(
    TEST_COM_URI,
    "simple1.js",
    "simple2.js"
  );
  const {
    selectors: { getSelectedSource },
  } = dbg;

  // Check threads
  await waitForElement(dbg, "threadsPaneItems");
  let threadsEl = findAllElements(dbg, "threadsPaneItems");
  is(threadsEl.length, 2, "There are two threads in the thread panel");
  ok(
    Array.from(threadsEl).every(
      el => !isThreadElementPaused(el) && !getThreadElementPausedBadge(el)
    ),
    "No threads are paused"
  );

  // Add breakpoint within the iframe, which is hit early on load
  await selectSource(dbg, "simple2.js");
  await addBreakpoint(dbg, "simple2.js", 7);

  const onBreakpoint = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  info("Reload the page to hit the breakpoint on load");
  const onReloaded = reload(dbg);
  await onBreakpoint;
  await waitForSelectedSource(dbg, "simple2.js");

  ok(
    getSelectedSource().url.includes("simple2.js"),
    "Selected source is simple2.js"
  );
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "simple2.js").id, 7);

  await waitForElement(dbg, "threadsPaneItems");
  threadsEl = findAllElements(dbg, "threadsPaneItems");
  is(threadsEl.length, 2, "There are two threads in the thread panel");
  const [mainThreadEl, remoteThreadEl] = threadsEl;
  is(
    mainThreadEl.textContent,
    "Main Thread",
    "first thread displayed is the main thread"
  );
  ok(
    !isThreadElementPaused(mainThreadEl),
    "Main Thread does not have the paused styling"
  );
  ok(
    !getThreadElementPausedBadge(mainThreadEl),
    "Main Thread does not have a paused badge"
  );

  ok(
    remoteThreadEl.textContent.startsWith(URL_ROOT_ORG_SSL),
    "second thread displayed is the remote thread"
  );
  ok(
    isThreadElementPaused(remoteThreadEl),
    "paused thread has the paused styling"
  );
  is(
    getThreadElementPausedBadge(remoteThreadEl).textContent,
    "paused",
    "paused badge is displayed in the remote thread item"
  );

  await stepIn(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "simple2.js").id, 7);

  // We can't used `stepIn` helper as this last step will resume
  // and the helper is expecting to pause again
  await dbg.actions.stepIn();
  assertNotPaused(dbg, "Stepping in two times resumes");

  info("Wait for reload to complete after resume");
  await onReloaded;

  await dbg.toolbox.closeToolbox();
});

function isThreadElementPaused(el) {
  return el.classList.contains("paused");
}

function getThreadElementPausedBadge(el) {
  return el.querySelector(".pause-badge");
}
