/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the POST requests display the correct information in the UI,
 * for raw payloads with attached content-type headers.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(POST_RAW_URL);
  info("Starting test... ");

  let { document, EVENTS, L10N, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;
  NetworkDetails._params.lazyEmpty = false;

  let wait = waitForNetworkEvents(monitor, 0, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  let onEvent = monitor.panelWin.once(EVENTS.TAB_UPDATED);
  NetMonitorView.toggleDetailsPane({ visible: true }, 2);
  RequestsMenu.selectedIndex = 0;
  yield onEvent;

  let tabEl = document.querySelectorAll("#event-details-pane tab")[2];
  let tabpanel = document.querySelectorAll("#event-details-pane tabpanel")[2];

  is(tabEl.getAttribute("selected"), "true",
    "The params tab in the network details pane should be selected.");

  is(tabpanel.querySelector("#request-params-box")
    .hasAttribute("hidden"), false,
    "The request params box doesn't have the indended visibility.");
  is(tabpanel.querySelector("#request-post-data-textarea-box")
    .hasAttribute("hidden"), true,
    "The request post data textarea box doesn't have the indended visibility.");

  is(tabpanel.querySelectorAll(".variables-view-scope").length, 1,
    "There should be 1 param scopes displayed in this tabpanel.");
  is(tabpanel.querySelectorAll(".variables-view-empty-notice").length, 0,
    "The empty notice should not be displayed in this tabpanel.");

  let postScope = tabpanel.querySelectorAll(".variables-view-scope")[0];
  is(postScope.querySelector(".name").getAttribute("value"),
    L10N.getStr("paramsFormData"),
    "The post scope doesn't have the correct title.");

  is(postScope.querySelectorAll(".variables-view-variable").length, 2,
    "There should be 2 param values displayed in the post scope.");
  is(postScope.querySelectorAll(".variables-view-variable .name")[0]
    .getAttribute("value"),
    "foo", "The first query param name was incorrect.");
  is(postScope.querySelectorAll(".variables-view-variable .value")[0]
    .getAttribute("value"),
    "\"bar\"", "The first query param value was incorrect.");
  is(postScope.querySelectorAll(".variables-view-variable .name")[1]
    .getAttribute("value"),
    "baz", "The second query param name was incorrect.");
  is(postScope.querySelectorAll(".variables-view-variable .value")[1]
    .getAttribute("value"),
    "\"123\"", "The second query param value was incorrect.");

  return teardown(monitor);
});
