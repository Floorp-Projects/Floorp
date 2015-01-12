/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test a http -> https redirect shows secure icon only for redirected https
 * request.
 */

add_task(function* () {
  let [tab, debuggee, monitor] = yield initNetMonitor(CUSTOM_GET_URL);
  let { $, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  debuggee.performRequests(1, HTTPS_REDIRECT_SJS);
  yield waitForNetworkEvents(monitor, 2);

  is(RequestsMenu.itemCount, 2, "There were two requests due to redirect.");

  let initial = RequestsMenu.items[0];
  let redirect = RequestsMenu.items[1];

  let initialSecurityIcon = $(".requests-security-state-icon", initial.target);
  let redirectSecurityIcon = $(".requests-security-state-icon", redirect.target);

  ok(initialSecurityIcon.classList.contains("security-state-insecure"),
     "Initial request was marked insecure.");

  ok(redirectSecurityIcon.classList.contains("security-state-secure"),
     "Redirected request was marked secure.");

  yield teardown(monitor);
});
