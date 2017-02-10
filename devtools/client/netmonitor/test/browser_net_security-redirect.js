/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test a http -> https redirect shows secure icon only for redirected https
 * request.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  let { $, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 2);
  yield ContentTask.spawn(tab.linkedBrowser, HTTPS_REDIRECT_SJS, function* (url) {
    content.wrappedJSObject.performRequests(1, url);
  });
  yield wait;

  is(RequestsMenu.itemCount, 2, "There were two requests due to redirect.");

  let initial = RequestsMenu.getItemAtIndex(0);
  let redirect = RequestsMenu.getItemAtIndex(1);

  let initialSecurityIcon =
    $(".requests-security-state-icon", getItemTarget(RequestsMenu, initial));
  let redirectSecurityIcon =
    $(".requests-security-state-icon", getItemTarget(RequestsMenu, redirect));

  ok(initialSecurityIcon.classList.contains("security-state-insecure"),
     "Initial request was marked insecure.");

  ok(redirectSecurityIcon.classList.contains("security-state-secure"),
     "Redirected request was marked secure.");

  yield teardown(monitor);
});
