/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around NETWORK_EVENT

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

// We are borrowing tests from the netmonitor frontend
const NETMONITOR_TEST_FOLDER =
  "https://example.com/browser/devtools/client/netmonitor/test/";
const CSP_URL = `${NETMONITOR_TEST_FOLDER}html_csp-test-page.html`;
const JS_CSP_URL = `${NETMONITOR_TEST_FOLDER}js_websocket-worker-test.js`;
const CSS_CSP_URL = `${NETMONITOR_TEST_FOLDER}internal-loaded.css`;

const CSP_BLOCKED_REASON_CODE = 4000;

add_task(async function testContentProcessRequests() {
  info(`Tests for NETWORK_EVENT resources fired from the content process`);

  const expectedAvailable = [
    {
      url: CSP_URL,
      method: "GET",
      isNavigationRequest: true,
      chromeContext: false,
    },
    {
      url: JS_CSP_URL,
      method: "GET",
      blockedReason: CSP_BLOCKED_REASON_CODE,
      isNavigationRequest: false,
      chromeContext: false,
    },
    {
      url: CSS_CSP_URL,
      method: "GET",
      blockedReason: CSP_BLOCKED_REASON_CODE,
      isNavigationRequest: false,
      chromeContext: false,
    },
  ];
  const expectedUpdated = [
    {
      url: CSP_URL,
      method: "GET",
      isNavigationRequest: true,
      chromeContext: false,
    },
    {
      url: JS_CSP_URL,
      method: "GET",
      blockedReason: CSP_BLOCKED_REASON_CODE,
      isNavigationRequest: false,
      chromeContext: false,
    },
    {
      url: CSS_CSP_URL,
      method: "GET",
      blockedReason: CSP_BLOCKED_REASON_CODE,
      isNavigationRequest: false,
      chromeContext: false,
    },
  ];

  await assertNetworkResourcesOnPage(
    CSP_URL,
    expectedAvailable,
    expectedUpdated
  );
});

add_task(async function testCanceledRequest() {
  info(`Tests for NETWORK_EVENT resources with a canceled request`);

  // Do a XHR request that we cancel against a slow loading page
  const requestUrl =
    "https://example.org/document-builder.sjs?delay=1000&html=foo";
  const html =
    "<!DOCTYPE html><script>(" +
    function(xhrUrl) {
      const xhr = new XMLHttpRequest();
      xhr.open("GET", xhrUrl);
      xhr.send(null);
    } +
    ")(" +
    JSON.stringify(requestUrl) +
    ")</script>";
  const pageUrl =
    "https://example.org/document-builder.sjs?html=" + encodeURIComponent(html);

  const expectedAvailable = [
    {
      url: pageUrl,
      method: "GET",
      isNavigationRequest: true,
      chromeContext: false,
    },
    {
      url: requestUrl,
      method: "GET",
      isNavigationRequest: false,
      blockedReason: "NS_BINDING_ABORTED",
      chromeContext: false,
    },
  ];
  const expectedUpdated = [
    {
      url: pageUrl,
      method: "GET",
      isNavigationRequest: true,
      chromeContext: false,
    },
    {
      url: requestUrl,
      method: "GET",
      isNavigationRequest: false,
      blockedReason: "NS_BINDING_ABORTED",
      chromeContext: false,
    },
  ];

  // Register a one-off listener to cancel the XHR request
  // Using XMLHttpRequest's abort() method from the content process
  // isn't reliable and would introduce many race condition in the test.
  // Canceling the request via nsIRequest.cancel privileged method,
  // from the parent process is much more reliable.
  const observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(subject, topic, data) {
      subject = subject.QueryInterface(Ci.nsIHttpChannel);
      if (subject.URI.spec == requestUrl) {
        subject.cancel(Cr.NS_BINDING_ABORTED);
        Services.obs.removeObserver(observer, "http-on-modify-request");
      }
    },
  };
  Services.obs.addObserver(observer, "http-on-modify-request");

  await assertNetworkResourcesOnPage(
    pageUrl,
    expectedAvailable,
    expectedUpdated
  );
});

add_task(async function testIframeRequest() {
  info(`Tests for NETWORK_EVENT resources with an iframe`);

  // Do a XHR request that we cancel against a slow loading page
  const iframeRequestUrl =
    "https://example.org/document-builder.sjs?html=iframe-request";
  const iframeHtml = `iframe<script>fetch("${iframeRequestUrl}")</script>`;
  const iframeUrl =
    "https://example.org/document-builder.sjs?html=" +
    encodeURIComponent(iframeHtml);
  const html = `top-document<iframe src="${iframeUrl}"></iframe>`;
  const pageUrl =
    "https://example.org/document-builder.sjs?html=" + encodeURIComponent(html);

  const expectedAvailable = [
    {
      url: pageUrl,
      method: "GET",
      chromeContext: false,
      isNavigationRequest: true,
      // The top level navigation request relates to the previous top level target.
      // Unfortunately, it is hard to test because it is racy.
      // The target front might be destroyed and `targetFront.url` will be null.
      // Or not just yet and be equal to "about:blank".
    },
    {
      url: iframeUrl,
      method: "GET",
      isNavigationRequest: false,
      targetFrontUrl: pageUrl,
      chromeContext: false,
    },
    {
      url: iframeRequestUrl,
      method: "GET",
      isNavigationRequest: false,
      targetFrontUrl: iframeUrl,
      chromeContext: false,
    },
  ];
  const expectedUpdated = [
    {
      url: pageUrl,
      method: "GET",
      isNavigationRequest: true,
      chromeContext: false,
    },
    {
      url: iframeUrl,
      method: "GET",
      isNavigationRequest: false,
      chromeContext: false,
    },
    {
      url: iframeRequestUrl,
      method: "GET",
      isNavigationRequest: false,
      chromeContext: false,
    },
  ];

  await assertNetworkResourcesOnPage(
    pageUrl,
    expectedAvailable,
    expectedUpdated
  );
});

async function assertNetworkResourcesOnPage(
  url,
  expectedAvailable,
  expectedUpdated
) {
  // First open a blank document to avoid spawning any request
  const tab = await addTab("about:blank");

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();
  const { resourceCommand } = commands;

  const onAvailable = resources => {
    for (const resource of resources) {
      // Immediately assert the resource, as the same resource object
      // will be notified to onUpdated and so if we assert it later
      // we will not highlight attributes that aren't set yet from onAvailable.
      const idx = expectedAvailable.findIndex(e => e.url === resource.url);
      ok(
        idx != -1,
        "Found a matching available notification for: " + resource.url
      );
      // Remove the match from the list in case there is many requests with the same url
      const [expected] = expectedAvailable.splice(idx, 1);

      assertResources(resource, expected);
    }
  };
  const onUpdated = updates => {
    for (const { resource } of updates) {
      const idx = expectedUpdated.findIndex(e => e.url === resource.url);
      ok(
        idx != -1,
        "Found a matching updated notification for: " + resource.url
      );
      // Remove the match from the list in case there is many requests with the same url
      const [expected] = expectedUpdated.splice(idx, 1);

      assertResources(resource, expected);
    }
  };

  // Start observing for network events before loading the test page
  await resourceCommand.watchResources([resourceCommand.TYPES.NETWORK_EVENT], {
    onAvailable,
    onUpdated,
  });

  // Load the test page that fires network requests
  const onLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url);
  await onLoaded;

  // Make sure we processed all the expected request updates
  await waitFor(
    () => !expectedAvailable.length,
    "Wait for all expected available notifications"
  );
  await waitFor(
    () => !expectedUpdated.length,
    "Wait for all expected updated notifications"
  );

  resourceCommand.unwatchResources([resourceCommand.TYPES.NETWORK_EVENT], {
    onAvailable,
    onUpdated,
  });

  await commands.destroy();

  BrowserTestUtils.removeTab(tab);
}

function assertResources(actual, expected) {
  is(
    actual.resourceType,
    ResourceCommand.TYPES.NETWORK_EVENT,
    "The resource type is correct"
  );
  is(
    typeof actual.innerWindowId,
    "number",
    "All requests have an innerWindowId attribute"
  );
  ok(
    actual.targetFront.isTargetFront,
    "All requests have a targetFront attribute"
  );

  for (const name in expected) {
    if (name == "targetFrontUrl") {
      is(
        actual.targetFront.url,
        expected[name],
        "The request matches the right target front"
      );
    } else {
      is(actual[name], expected[name], `The '${name}' attribute is correct`);
    }
  }
}
