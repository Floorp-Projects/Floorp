/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = URL_ROOT + "doc_network-observer.html";
const REQUEST_URL =
  URL_ROOT + `sjs_network-observer-test-server.sjs?sts=200&fmt=html`;

// Check that the NetworkObserver can detect basic requests and calls the
// onNetworkEvent callback when expected.
add_task(async function testSingleRequest() {
  await addTab(TEST_URL);

  const onNetworkEvents = waitForNetworkEvents(REQUEST_URL, 1);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [REQUEST_URL], _url => {
    content.wrappedJSObject.fetch(_url);
  });

  const events = await onNetworkEvents;
  is(events.length, 1, "Received the expected number of network events");
});

add_task(async function testMultipleRequests() {
  await addTab(TEST_URL);
  const EXPECTED_REQUESTS_COUNT = 5;

  const onNetworkEvents = waitForNetworkEvents(
    REQUEST_URL,
    EXPECTED_REQUESTS_COUNT
  );
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [REQUEST_URL, EXPECTED_REQUESTS_COUNT],
    (_url, _count) => {
      for (let i = 0; i < _count; i++) {
        content.wrappedJSObject.fetch(_url);
      }
    }
  );

  const events = await onNetworkEvents;
  is(
    events.length,
    EXPECTED_REQUESTS_COUNT,
    "Received the expected number of network events"
  );
});

add_task(async function testOnNetworkEventArguments() {
  await addTab(TEST_URL);

  const onNetworkEvent = new Promise(resolve => {
    const networkObserver = new NetworkObserver({
      ignoreChannelFunction: () => false,
      onNetworkEvent: (...args) => {
        resolve(args);
        return createNetworkEventOwner();
      },
    });
    registerCleanupFunction(() => networkObserver.destroy());
  });

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [REQUEST_URL], _url => {
    content.wrappedJSObject.fetch(_url);
  });

  const args = await onNetworkEvent;
  is(args.length, 2, "Received two arguments");
  is(typeof args[0], "object", "First argument is an object");
  ok(args[1] instanceof Ci.nsIChannel, "Second argument is a channel");
});
