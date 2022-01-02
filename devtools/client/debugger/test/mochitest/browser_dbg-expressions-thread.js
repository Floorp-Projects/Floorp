/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Test the watch expressions update when selecting a different thread in the thread panel.
 */

"use strict";

const TEST_COM_URI =
  URL_ROOT_COM + "examples/doc_dbg-fission-frame-sources.html";

add_task(async function() {
  // Load a test page with a remote frame and wait for both sources to be visible.
  // simple1.js is imported by the main page. simple2.js comes from the frame.
  const dbg = await initDebuggerWithAbsoluteURL(
    TEST_COM_URI,
    "simple1.js",
    "simple2.js"
  );

  // expand threads pane
  const threadsPaneEl = await waitForElementWithSelector(
    dbg,
    ".threads-pane .header-label"
  );
  threadsPaneEl.click();

  await waitForElement(dbg, "threadsPaneItems");
  const threadsEl = findAllElements(dbg, "threadsPaneItems");
  is(threadsEl.length, 2, "There are two threads in the thread panel");
  const [mainThreadEl, remoteThreadEl] = threadsEl;
  is(
    mainThreadEl.textContent,
    "Main Thread",
    "first thread displayed is the main thread"
  );
  is(
    remoteThreadEl.textContent,
    "Test remote frame sources",
    "second thread displayed is the remote thread"
  );

  await addExpression(dbg, "document.location.host");

  is(
    getWatchExpressionValue(dbg, 1),
    `"example.com"`,
    "expression is evaluated on the expected thread"
  );

  info(
    "Select the remote frame thread and check that the expression is updated"
  );
  let onExpressionsEvaluated = waitForDispatch(
    dbg.store,
    "EVALUATE_EXPRESSIONS"
  );
  remoteThreadEl.click();
  await onExpressionsEvaluated;

  is(
    getWatchExpressionValue(dbg, 1),
    `"example.org"`,
    "expression is evaluated on the remote origin thread"
  );

  info("Select the main thread again and check that the expression is updated");
  onExpressionsEvaluated = waitForDispatch(dbg.store, "EVALUATE_EXPRESSIONS");
  mainThreadEl.click();
  await onExpressionsEvaluated;

  is(
    getWatchExpressionValue(dbg, 1),
    `"example.com"`,
    "expression is evaluated on the main thread again"
  );

  // close the threads pane so following test don't have it open
  threadsPaneEl.click();
});
