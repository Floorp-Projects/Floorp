/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that correct security state indicator appears depending on the security
 * state.
 */

add_task(function* () {
  const EXPECTED_SECURITY_STATES = {
    "test1.example.com": "security-state-insecure",
    "example.com": "security-state-secure",
    "nocert.example.com": "security-state-broken",
    "localhost": "security-state-local",
  };

  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  let { $, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  yield performRequests();

  for (let item of RequestsMenu.items) {
    let domain = $(".requests-menu-domain", item.target).value;

    info("Found a request to " + domain);
    ok(domain in EXPECTED_SECURITY_STATES, "Domain " + domain + " was expected.");

    let classes = $(".requests-security-state-icon", item.target).classList;
    let expectedClass = EXPECTED_SECURITY_STATES[domain];

    info("Classes of security state icon are: " + classes);
    info("Security state icon is expected to contain class: " + expectedClass);
    ok(classes.contains(expectedClass), "Icon contained the correct class name.");
  }

  return teardown(monitor);

  /**
   * A helper that performs requests to
   *  - https://nocert.example.com (broken)
   *  - https://example.com (secure)
   *  - http://test1.example.com (insecure)
   *  - http://localhost (local)
   * and waits until NetworkMonitor has handled all packets sent by the server.
   */
  function* performRequests() {
    function executeRequests(count, url) {
      return ContentTask.spawn(tab.linkedBrowser, {count, url}, function* (args) {
        content.wrappedJSObject.performRequests(args.count, args.url);
      });
    }

    // waitForNetworkEvents does not work for requests with security errors as
    // those only emit 9/13 events of a successful request.
    let done = waitForSecurityBrokenNetworkEvent();

    info("Requesting a resource that has a certificate problem.");
    yield executeRequests(1, "https://nocert.example.com");

    // Wait for the request to complete before firing another request. Otherwise
    // the request with security issues interfere with waitForNetworkEvents.
    info("Waiting for request to complete.");
    yield done;

    // Next perform a request over HTTP. If done the other way around the latter
    // occasionally hangs waiting for event timings that don't seem to appear...
    done = waitForNetworkEvents(monitor, 1);
    info("Requesting a resource over HTTP.");
    yield executeRequests(1, "http://test1.example.com" + CORS_SJS_PATH);
    yield done;

    done = waitForNetworkEvents(monitor, 1);
    info("Requesting a resource over HTTPS.");
    yield executeRequests(1, "https://example.com" + CORS_SJS_PATH);
    yield done;

    done = waitForSecurityBrokenNetworkEvent(true);
    info("Requesting a resource over HTTP to localhost.");
    yield executeRequests(1, "http://localhost" + CORS_SJS_PATH);
    yield done;

    const expectedCount = Object.keys(EXPECTED_SECURITY_STATES).length;
    is(RequestsMenu.itemCount, expectedCount, expectedCount + " events logged.");
  }

  /**
   * Returns a promise that's resolved once a request with security issues is
   * completed.
   */
  function waitForSecurityBrokenNetworkEvent(networkError) {
    let awaitedEvents = [
      "UPDATING_REQUEST_HEADERS",
      "RECEIVED_REQUEST_HEADERS",
      "UPDATING_REQUEST_COOKIES",
      "RECEIVED_REQUEST_COOKIES",
      "STARTED_RECEIVING_RESPONSE",
      "UPDATING_RESPONSE_CONTENT",
      "RECEIVED_RESPONSE_CONTENT",
      "UPDATING_EVENT_TIMINGS",
      "RECEIVED_EVENT_TIMINGS",
    ];

    // If the reason for breakage is a network error, then the
    // STARTED_RECEIVING_RESPONSE event does not fire.
    if (networkError) {
      awaitedEvents = awaitedEvents.filter(e => e !== "STARTED_RECEIVING_RESPONSE");
    }

    let promises = awaitedEvents.map((event) => {
      return monitor.panelWin.once(EVENTS[event]);
    });

    return Promise.all(promises);
  }
});
