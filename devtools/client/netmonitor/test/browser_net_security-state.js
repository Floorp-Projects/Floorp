/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that correct security state indicator appears depending on the security
 * state.
 */

add_task(async function() {
  const EXPECTED_SECURITY_STATES = {
    "test1.example.com": "security-state-insecure",
    "example.com": "security-state-secure",
    "nocert.example.com": "security-state-broken",
    "localhost": "security-state-local",
  };

  const { tab, monitor } = await initNetMonitor(CUSTOM_GET_URL);
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  await performRequests();

  for (const subitemNode of Array.from(document.querySelectorAll(
    "requests-list-column.requests-list-security-and-domain"))) {
    const domain = subitemNode.querySelector(".requests-list-domain").textContent;

    info("Found a request to " + domain);
    ok(domain in EXPECTED_SECURITY_STATES, "Domain " + domain + " was expected.");

    const classes = subitemNode.querySelector(".requests-security-state-icon").classList;
    const expectedClass = EXPECTED_SECURITY_STATES[domain];

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
  async function performRequests() {
    function executeRequests(count, url) {
      return ContentTask.spawn(tab.linkedBrowser, {count, url}, async function(args) {
        content.wrappedJSObject.performRequests(args.count, args.url);
      });
    }

    let done = waitForNetworkEvents(monitor, 1);
    info("Requesting a resource that has a certificate problem.");
    await executeRequests(1, "https://nocert.example.com");

    // Wait for the request to complete before firing another request. Otherwise
    // the request with security issues interfere with waitForNetworkEvents.
    info("Waiting for request to complete.");
    await done;

    // Next perform a request over HTTP. If done the other way around the latter
    // occasionally hangs waiting for event timings that don't seem to appear...
    done = waitForNetworkEvents(monitor, 1);
    info("Requesting a resource over HTTP.");
    await executeRequests(1, "http://test1.example.com" + CORS_SJS_PATH);
    await done;

    done = waitForNetworkEvents(monitor, 1);
    info("Requesting a resource over HTTPS.");
    await executeRequests(1, "https://example.com" + CORS_SJS_PATH);
    await done;

    done = waitForNetworkEvents(monitor, 1);
    info("Requesting a resource over HTTP to localhost.");
    await executeRequests(1, "http://localhost" + CORS_SJS_PATH);
    await done;

    const expectedCount = Object.keys(EXPECTED_SECURITY_STATES).length;
    is(store.getState().requests.requests.size,
      expectedCount,
      expectedCount + " events logged.");
  }
});
