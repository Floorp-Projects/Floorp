/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that all the expected service worker requests are received
// by the network observer.
add_task(async function testServiceWorkerSuccessRequests() {
  await addTab(URL_ROOT + "doc_network-observer.html");

  const REQUEST_URL =
    URL_ROOT + `sjs_network-observer-test-server.sjs?sts=200&fmt=`;

  const EXPECTED_REQUESTS = [
    // The main service worker script request
    `https://example.com/browser/devtools/shared/network-observer/test/browser/serviceworker.js`,
    // The requests intercepted by the service worker
    REQUEST_URL + "js",
    REQUEST_URL + "css",
    // The request initiated by the service worker
    REQUEST_URL + "json",
  ];

  const onNetworkEvents = waitForNetworkEvents(null, 4);

  info("Register the service worker and send requests...");
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [REQUEST_URL],
    async url => {
      await content.wrappedJSObject.registerServiceWorker();
      content.wrappedJSObject.fetch(url + "js");
      content.wrappedJSObject.fetch(url + "css");
    }
  );
  const events = await onNetworkEvents;

  is(events.length, 4, "Received the expected number of network events");
  for (const { options, channel } of events) {
    info(`Assert the info for the request from ${channel.URI.spec}`);
    ok(
      EXPECTED_REQUESTS.includes(channel.URI.spec),
      `The request for ${channel.URI.spec} is an expected service worker request`
    );
    ok(
      channel.loadInfo.browsingContextID !== 0,
      `The request for ${channel.URI.spec} has a Browsing Context ID of ${channel.loadInfo.browsingContextID}`
    );
    // The main service worker script request is not from the service worker
    if (channel.URI.spec.includes("serviceworker.js")) {
      ok(
        !options.fromServiceWorker,
        `The request for ${channel.URI.spec} is not from the service worker\n`
      );
    } else {
      ok(
        options.fromServiceWorker,
        `The request for ${channel.URI.spec} is from the service worker\n`
      );
    }
  }

  info("Unregistering the service worker...");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    await content.wrappedJSObject.unregisterServiceWorker();
  });
});

// Tests that the expected failed service worker request is received by the network observer.
add_task(async function testServiceWorkerFailedRequests() {
  await addTab(URL_ROOT + "doc_network-observer-missing-service-worker.html");

  const REQUEST_URL =
    URL_ROOT + `sjs_network-observer-test-server.sjs?sts=200&fmt=js`;

  const EXPECTED_REQUESTS = [
    // The main service worker script request which should be missing
    "https://example.com/browser/devtools/shared/network-observer/test/browser/serviceworker-missing.js",
    // A notrmal request
    "https://example.com/browser/devtools/shared/network-observer/test/browser/sjs_network-observer-test-server.sjs?sts=200&fmt=js",
  ];

  const onNetworkEvents = waitForNetworkEvents(null, 2);
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [REQUEST_URL],
    async url => {
      await content.wrappedJSObject.registerServiceWorker();
      content.wrappedJSObject.fetch(url);
    }
  );

  const events = await onNetworkEvents;
  is(events.length, 2, "Received the expected number of network events");

  for (const { options, channel } of events) {
    info(`Assert the info for the request from ${channel.URI.spec}`);
    ok(
      EXPECTED_REQUESTS.includes(channel.URI.spec),
      `The request for ${channel.URI.spec} is an expected request`
    );
    ok(
      channel.loadInfo.browsingContextID !== 0,
      `The request for ${channel.URI.spec} has a Browsing Context ID of ${channel.loadInfo.browsingContextID}`
    );
    ok(
      !options.fromServiceWorker,
      `The request for ${channel.URI.spec} is not from the service worker\n`
    );
  }
});
