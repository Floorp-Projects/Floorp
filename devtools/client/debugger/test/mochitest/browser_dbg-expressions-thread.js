/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Test the watch expressions update when selecting a different thread in the thread panel.
 */

"use strict";

const TEST_COM_URI = `${URL_ROOT_COM_SSL}examples/doc_dbg-fission-frame-sources.html`;
const TEST_ORG_IFRAME_URI = `${URL_ROOT_ORG_SSL}examples/doc_dbg-fission-frame-sources-frame.html`;
const DATA_URI = "data:text/html,<title>foo</title>";

add_task(async function () {
  // Make sure that the thread section is expanded
  await pushPref("devtools.debugger.threads-visible", true);

  // Load a test page with a remote frame and wait for both sources to be visible.
  // simple1.js is imported by the main page. simple2.js comes from the frame.
  const dbg = await initDebuggerWithAbsoluteURL(
    TEST_COM_URI,
    "simple1.js",
    "simple2.js"
  );

  const threadsPaneEl = await waitForElementWithSelector(
    dbg,
    ".threads-pane .header-label"
  );

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

  await addExpression(dbg, "document.location.href");

  is(
    getWatchExpressionValue(dbg, 1),
    JSON.stringify(TEST_COM_URI),
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
    JSON.stringify(TEST_ORG_IFRAME_URI),
    "expression is evaluated on the remote origin thread"
  );

  info("Select the main thread again and check that the expression is updated");
  onExpressionsEvaluated = waitForDispatch(dbg.store, "EVALUATE_EXPRESSIONS");
  mainThreadEl.click();
  await onExpressionsEvaluated;

  is(
    getWatchExpressionValue(dbg, 1),
    JSON.stringify(TEST_COM_URI),
    "expression is evaluated on the main thread again"
  );

  // close the threads pane so following test don't have it open
  threadsPaneEl.click();

  await navigateToAbsoluteURL(dbg, DATA_URI);

  is(
    getWatchExpressionValue(dbg, 1),
    JSON.stringify(DATA_URI),
    "The location.host expression is updated after a navigaiton"
  );

  await addExpression(dbg, "document.title");

  is(
    getWatchExpressionValue(dbg, 2),
    `"foo"`,
    "We can add expressions after a navigation"
  );
});
