/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// A test to ensure that the content in details pane is not duplicated.

let test = Task.async(function* () {
  info("Initializing test");
  let [tab, debuggee, monitor] = yield initNetMonitor(CUSTOM_GET_URL);
  let panel = monitor.panelWin;
  let { NetMonitorView, EVENTS } = panel;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;

  const COOKIE_UNIQUE_PATH = "/do-not-use-in-other-tests-using-cookies";

  let TEST_CASES = [
    {
      desc: "Test headers tab",
      pageURI: CUSTOM_GET_URL,
      requestURI: null,
      isPost: false,
      tabIndex: 0,
      variablesView: NetworkDetails._headers,
      expectedScopeLength: 2,
    },
    {
      desc: "Test cookies tab",
      pageURI: CUSTOM_GET_URL,
      requestURI: COOKIE_UNIQUE_PATH,
      isPost: false,
      tabIndex: 1,
      variablesView: NetworkDetails._cookies,
      expectedScopeLength: 1,
    },
    {
      desc: "Test params tab",
      pageURI: POST_RAW_URL,
      requestURI: null,
      isPost: true,
      tabIndex: 2,
      variablesView: NetworkDetails._params,
      expectedScopeLength: 1,
    },
  ];

  info("Adding a cookie for the \"Cookie\" tab test");
  debuggee.document.cookie = "a=b; path=" + COOKIE_UNIQUE_PATH;

  info("Running tests");
  for (let spec of TEST_CASES) {
    yield runTestCase(spec);
  }

  // Remove the cookie. If an error occurs the path of the cookie ensures it
  // doesn't mess with the other tests.
  info("Removing the added cookie.");
  debuggee.document.cookie = "a=; expires=Thu, 01 Jan 1970 00:00:00 GMT; path=" +
    COOKIE_UNIQUE_PATH;

  yield teardown(monitor);
  finish();

  /**
   * A helper that handles the execution of each case.
   */
  function* runTestCase(spec) {
    info("Running case: " + spec.desc);
    debuggee.content.location = spec.pageURI;

    yield waitForNetworkEvents(monitor, 1);
    RequestsMenu.clear();
    yield waitForFinalDetailTabUpdate(spec.tabIndex, spec.isPost, spec.requestURI);

    is(spec.variablesView._store.length, spec.expectedScopeLength,
       "View contains " + spec.expectedScopeLength + " scope headers");
  }

  /**
   * A helper that prepares the variables view for the actual testing. It
   * - selects the correct tab
   * - performs the specified request to specified URI
   * - opens the details view
   * - waits for the final update to happen
   */
  function* waitForFinalDetailTabUpdate(tabIndex, isPost, uri) {
    let onNetworkEvent = waitFor(panel, EVENTS.NETWORK_EVENT);
    let onDetailsPopulated = waitFor(panel, EVENTS.NETWORKDETAILSVIEW_POPULATED);
    let onRequestFinished = isPost ?
      waitForNetworkEvents(monitor, 0, 1) : waitForNetworkEvents(monitor, 1);

    info("Performing a request");
    debuggee.performRequests(1, uri);

    info("Waiting for NETWORK_EVENT");
    yield onNetworkEvent;

    ok(true, "Received NETWORK_EVENT. Selecting the item.");
    let item = RequestsMenu.getItemAtIndex(0);
    RequestsMenu.selectedItem = item;

    info("Item selected. Waiting for NETWORKDETAILSVIEW_POPULATED");
    yield onDetailsPopulated;

    info("Selecting tab at index " + tabIndex);
    NetworkDetails.widget.selectedIndex = tabIndex;

    ok(true, "Received NETWORKDETAILSVIEW_POPULATED. Waiting for request to finish");
    yield onRequestFinished;

    ok(true, "Request finished. Waiting for tab update to complete");
    let onDetailsUpdateFinished = waitFor(panel, EVENTS.TAB_UPDATED);
    yield onDetailsUpdateFinished;
    ok(true, "Details were updated");
  }
});
