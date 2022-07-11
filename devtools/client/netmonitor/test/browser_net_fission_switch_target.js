/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test switching for the top-level target.

const EXAMPLE_COM_URL = "https://example.com/document-builder.sjs?html=testcom";
const EXAMPLE_NET_URL = "https://example.net/document-builder.sjs?html=testnet";
const REQUEST_URL = HTTPS_SEARCH_SJS + "?value=test";
const PARENT_PROCESS_URL = "about:blank";

add_task(async function() {
  info("Open a page that runs on the content process and the netmonitor");
  const { monitor } = await initNetMonitor(EXAMPLE_COM_URL, {
    requestCount: 1,
  });
  await assertRequest(monitor, REQUEST_URL);

  info("Navigate to a page that runs in another content process (if fission)");
  await waitForUpdatesAndNavigateTo(EXAMPLE_NET_URL);
  await assertRequest(monitor, REQUEST_URL);

  info("Navigate to a parent process page");
  await waitForUpdatesAndNavigateTo(PARENT_PROCESS_URL);
  await assertRequest(monitor, REQUEST_URL);

  info("Navigate back to the example.com content page");
  await waitForUpdatesAndNavigateTo(EXAMPLE_COM_URL);
  await assertRequest(monitor, REQUEST_URL);

  await teardown(monitor);
});

async function waitForUpdatesAndNavigateTo(url) {
  await waitForAllNetworkUpdateEvents();
  await navigateTo(url);
}

async function assertRequest(monitor, url) {
  const waitForRequests = waitForNetworkEvents(monitor, 1);
  info("Create a request in the target page for the URL: " + url);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [url], async _url => {
    // Note: we are not reusing performRequests available in some netmonitor
    // test pages because we also need to run this helper against an about:
    // page, which won't have the helper defined.
    // Therefore, we use a simplified performRequest implementation here.
    const xhr = new content.wrappedJSObject.XMLHttpRequest();
    xhr.open("GET", _url, true);
    xhr.send(null);
  });
  info("Wait for the request to be received by the netmonitor UI");
  return waitForRequests;
}
