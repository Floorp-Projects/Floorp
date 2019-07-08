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
  await ContentTask.spawn(tab.linkedBrowser, HTTPS_REDIRECT_SJS, async function(
    url
  ) {
    content.wrappedJSObject.performRequests(1, url);
  });
  await wait;

  is(
    store.getState().requests.requests.size,
    2,
    "There were two requests due to redirect."
  );

  const [
    initialDomainSecurityIcon,
    initialUrlSecurityIcon,
    redirectDomainSecurityIcon,
    redirectUrlSecurityIcon,
  ] = document.querySelectorAll(".requests-security-state-icon");

  ok(
    initialDomainSecurityIcon.classList.contains("security-state-insecure"),
    "Initial request was marked insecure for domain column."
  );

  ok(
    redirectDomainSecurityIcon.classList.contains("security-state-secure"),
    "Redirected request was marked secure for domain column."
  );

  ok(
    initialUrlSecurityIcon.classList.contains("security-state-insecure"),
    "Initial request was marked insecure for URL column."
  );

  ok(
    redirectUrlSecurityIcon.classList.contains("security-state-secure"),
    "Redirected request was marked secure for URL column."
  );

  await teardown(monitor);
});
