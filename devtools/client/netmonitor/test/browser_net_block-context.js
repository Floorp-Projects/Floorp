/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that context menus for blocked requests work
 */

add_task(async function () {
  const { monitor } = await initNetMonitor(HTTPS_SIMPLE_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  requestLongerTimeout(2);

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  info("Loading initial page");
  const wait = waitForNetworkEvents(monitor, 1);
  await navigateTo(HTTPS_SIMPLE_URL);
  await wait;

  info("Opening the blocked requests panel");
  document.querySelector(".requests-list-blocking-button").click();

  info("Adding sample block strings");
  const waitForBlockingContents = waitForDOM(
    document,
    ".request-blocking-contents"
  );
  await waitForBlockingAction(store, () => Actions.addBlockedUrl("test-page"));
  await waitForBlockingAction(store, () => Actions.addBlockedUrl("Two"));
  await waitForBlockingContents;

  is(getListitems(document), 2);

  info("Reloading page, URLs should be blocked in request list");
  await reloadPage(monitor, { isRequestBlocked: true });
  is(checkIfRequestIsBlocked(document), true);

  info("Disabling all blocked strings");
  await openMenuAndClick(
    monitor,
    store,
    document,
    "request-blocking-disable-all"
  );
  is(getCheckedCheckboxes(document), 0);

  info("Reloading page, URLs should not be blocked in request list");
  await reloadPage(monitor, { isRequestBlocked: false });

  is(checkIfRequestIsBlocked(document), false);

  info("Enabling all blocked strings");
  await openMenuAndClick(
    monitor,
    store,
    document,
    "request-blocking-enable-all"
  );
  is(getCheckedCheckboxes(document), 2);

  info("Reloading page, URLs should be blocked in request list");
  await reloadPage(monitor, { isRequestBlocked: true });

  is(checkIfRequestIsBlocked(document), true);

  info("Removing all blocked strings");
  await openMenuAndClick(
    monitor,
    store,
    document,
    "request-blocking-remove-all"
  );
  is(getListitems(document), 0);

  info("Reloading page, URLs should no longer be blocked in request list");
  await reloadPage(monitor, { isRequestBlocked: false });
  is(checkIfRequestIsBlocked(document), false);

  return teardown(monitor);
});

async function waitForBlockingAction(store, action) {
  const wait = waitForDispatch(store, "REQUEST_BLOCKING_UPDATE_COMPLETE");
  store.dispatch(action());
  await wait;
}

async function openMenuAndClick(monitor, store, document, itemSelector) {
  info(`Right clicking on white-space in the header to get the context menu`);
  EventUtils.sendMouseEvent(
    { type: "contextmenu" },
    document.querySelector(".request-blocking-contents")
  );

  const wait = waitForDispatch(store, "REQUEST_BLOCKING_UPDATE_COMPLETE");
  await selectContextMenuItem(monitor, itemSelector);
  await wait;
}

async function reloadPage(monitor, { isRequestBlocked = false } = {}) {
  const wait = waitForNetworkEvents(monitor, 1);
  if (isRequestBlocked) {
    // Note: Do not use navigateTo or reloadBrowser here as the request will
    // be blocked and no navigation happens
    gBrowser.selectedBrowser.reload();
  } else {
    await reloadBrowser();
  }
  await wait;
}

function getCheckedCheckboxes(document) {
  const checkboxes = [
    ...document.querySelectorAll(".request-blocking-contents li input"),
  ];
  return checkboxes.filter(checkbox => checkbox.checked).length;
}

function getListitems(document) {
  return document.querySelectorAll(".request-blocking-contents li").length;
}

function checkIfRequestIsBlocked(document) {
  const firstRequest = document.querySelectorAll(".request-list-item")[0];
  const blockedRequestSize = firstRequest.querySelector(
    ".requests-list-transferred"
  ).textContent;
  return blockedRequestSize.includes("Blocked");
}
