/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test a http -> https redirect shows secure icon only for redirected https
 * request.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CUSTOM_GET_URL);
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 2);
  await ContentTask.spawn(tab.linkedBrowser, HTTPS_REDIRECT_SJS, async function(url) {
    content.wrappedJSObject.performRequests(1, url);
  });
  await wait;

  is(store.getState().requests.requests.size, 2,
     "There were two requests due to redirect.");

  const [
    initialSecurityIcon,
    redirectSecurityIcon,
  ] = document.querySelectorAll(".requests-security-state-icon");

  ok(initialSecurityIcon.classList.contains("security-state-insecure"),
     "Initial request was marked insecure.");

  ok(redirectSecurityIcon.classList.contains("security-state-secure"),
     "Redirected request was marked secure.");

  await teardown(monitor);
});
