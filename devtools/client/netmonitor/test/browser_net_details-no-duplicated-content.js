/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A test to ensure that the content in details pane is not duplicated.

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
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
  yield setDocCookie("a=b; path=" + COOKIE_UNIQUE_PATH);

  info("Running tests");
  for (let spec of TEST_CASES) {
    yield runTestCase(spec);
  }

  // Remove the cookie. If an error occurs the path of the cookie ensures it
  // doesn't mess with the other tests.
  info("Removing the added cookie.");
  yield setDocCookie(
    "a=; expires=Thu, 01 Jan 1970 00:00:00 GMT; path=" + COOKIE_UNIQUE_PATH);

  yield teardown(monitor);

  /**
   * Set a content document cookie
   */
  function setDocCookie(cookie) {
    return ContentTask.spawn(tab.linkedBrowser, cookie, function* (cookieArg) {
      content.document.cookie = cookieArg;
    });
  }

  /**
   * A helper that handles the execution of each case.
   */
  function* runTestCase(spec) {
    info("Running case: " + spec.desc);
    let wait = waitForNetworkEvents(monitor, 1);
    tab.linkedBrowser.loadURI(spec.pageURI);
    yield wait;

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
    let onNetworkEvent = panel.once(EVENTS.NETWORK_EVENT);
    let onDetailsPopulated = panel.once(EVENTS.NETWORKDETAILSVIEW_POPULATED);
    let onRequestFinished = isPost ?
      waitForNetworkEvents(monitor, 0, 1) :
      waitForNetworkEvents(monitor, 1);

    info("Performing a request");
    yield ContentTask.spawn(tab.linkedBrowser, uri, function* (url) {
      content.wrappedJSObject.performRequests(1, url);
    });

    info("Waiting for NETWORK_EVENT");
    yield onNetworkEvent;

    if (!RequestsMenu.getItemAtIndex(0)) {
      info("Waiting for the request to be added to the view");
      yield monitor.panelWin.once(EVENTS.REQUEST_ADDED);
    }

    ok(true, "Received NETWORK_EVENT. Selecting the item.");
    let item = RequestsMenu.getItemAtIndex(0);
    RequestsMenu.selectedItem = item;

    info("Item selected. Waiting for NETWORKDETAILSVIEW_POPULATED");
    yield onDetailsPopulated;

    info("Received populated event. Selecting tab at index " + tabIndex);
    NetworkDetails.widget.selectedIndex = tabIndex;

    info("Waiting for request to finish.");
    yield onRequestFinished;

    ok(true, "Request finished.");

    /**
     * Because this test uses lazy updates there's four scenarios to consider:
     * #1: Everything is updated and test is ready to continue.
     * #2: There's updates that are waiting to be flushed.
     * #3: Updates are flushed but the tab update is still running.
     * #4: There's pending updates and a tab update is still running.
     *
     * For case #1 there's not going to be a TAB_UPDATED event so don't wait for
     * it (bug 1106181).
     *
     * For cases #2 and #3 it's enough to wait for one TAB_UPDATED event as for
     * - case #2 the next flush will perform the final update and single
     *   TAB_UPDATED event is emitted.
     * - case #3 the running update is the final update that'll emit one
     *   TAB_UPDATED event.
     *
     * For case #4 we must wait for the updates to be flushed before we can
     * start waiting for TAB_UPDATED event or we'll continue the test right
     * after the pending update finishes.
     */
    let hasQueuedUpdates = RequestsMenu._updateQueue.length !== 0;
    let hasRunningTabUpdate = NetworkDetails._viewState.updating[tabIndex];

    if (hasQueuedUpdates || hasRunningTabUpdate) {
      info("There's pending updates - waiting for them to finish.");
      info("  hasQueuedUpdates: " + hasQueuedUpdates);
      info("  hasRunningTabUpdate: " + hasRunningTabUpdate);

      if (hasQueuedUpdates && hasRunningTabUpdate) {
        info("Waiting for updates to be flushed.");
        // _flushRequests calls .populate which emits the following event
        yield panel.once(EVENTS.NETWORKDETAILSVIEW_POPULATED);

        info("Requests flushed.");
      }

      info("Waiting for final tab update.");
      yield waitFor(panel, EVENTS.TAB_UPDATED);
    }

    info("All updates completed.");
  }
});
