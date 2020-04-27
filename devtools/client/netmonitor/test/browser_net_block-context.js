/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that context menus for blocked requests work
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  requestLongerTimeout(2);

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  info("Loading initial page");
  const wait = waitForNetworkEvents(monitor, 1);
  await navigateTo(SIMPLE_URL);
  await wait;

  info("Opening the blocked requests panel");
  document.querySelector(".requests-list-blocking-button").click();

  info("Adding sample block strings");
  await waitForBlockingAction(store, () => Actions.addBlockedUrl("test-page"));
  await waitForBlockingAction(store, () => Actions.addBlockedUrl("Two"));
  is(getListitems(document), 2);

  info("Reloading page, URLs should be blocked in request list");
  await reloadPage(monitor, tab);
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
  await reloadPage(monitor, tab);
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
  await reloadPage(monitor, tab);
  is(checkIfRequestIsBlocked(document), true);

  info("Removing all blocked strings");
  await openMenuAndClick(
    monitor,
    store,
    document,
    "request-blocking-remove-all"
  );
  is(getListitems(document), 0);

  info("Reloading page, URLs should be blocked in request list");
  await reloadPage(monitor, tab);
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
  const node = getContextMenuItem(monitor, itemSelector);
  node.click();
  await wait;
}

async function reloadPage(monitor, tab) {
  const wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
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
