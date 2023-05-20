/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CACHE_TEST_URL = EXAMPLE_URL + "html_cache-test-page.html";

// Test the cache details panel.
add_task(async function () {
  const { monitor } = await initNetMonitor(CACHE_TEST_URL, {
    enableCache: true,
    requestCount: 1,
  });

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  info("Create a 200 request");
  let waitForRequest = waitForNetworkEvents(monitor, 1);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.sendRequestWithStatus("200");
  });
  await waitForRequest;

  info("Select the request and wait until the headers panel is displayed");
  store.dispatch(Actions.selectRequestByIndex(0));
  await waitFor(() => document.querySelector(".headers-overview"));
  ok(
    !document.querySelector("#cache-tab"),
    "No cache panel is available for the 200 request"
  );

  info("Create a 304 request");
  waitForRequest = waitForNetworkEvents(monitor, 1);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.sendRequestWithStatus("304");
  });
  await waitForRequest;

  info("Select the request and wait until the headers panel is displayed");
  store.dispatch(Actions.selectRequestByIndex(1));
  await waitFor(() => document.querySelector(".headers-overview"));
  ok(
    document.querySelector("#cache-tab"),
    "A cache panel is available for the 304 request"
  );
  document.querySelector("#cache-tab").click();

  info("Wait until the Cache panel content is displayed");
  await waitFor(() => !!document.getElementById("/Cache"));

  const device = getCacheDetailsValue(document, "Device");
  is(device, "Not Available", "device information is `Not Available`");

  // We cannot precisely assert the dates rendered by the cache panel because
  // they are formatted using toLocaleDateString/toLocaleTimeString, and
  // `new Date` might be unable to parse them. See Bug 1800448.

  // For "last modified" should be the same day as the test, and we could assert
  // that. However the cache panel is intermittently fully "Not available",
  // except for the "Expires" field, which seems to always have a value.
  const lastModified = getCacheDetailsValue(document, "Last Modified");
  info("Retrieved lastModified value: " + lastModified);
  ok(!!lastModified, "Last Modified was found in the cache panel");

  // For "expires" we will only check that this is not set to `Not Available`.
  const expires = getCacheDetailsValue(document, "Expires");
  info("Retrieved expires value: " + expires);
  ok(
    !expires.includes("Not Available"),
    "Expires is set to a value other than unavailable"
  );
});

/**
 * Helper to retrieve individual values from the Cache details panel.
 * Eg, for `Expires: "11/9/2022 6:54:33 PM"`, this should return
 * "11/9/2022 6:54:33 PM".
 *
 * @param {Document} doc
 *     The netmonitor document.
 * @param {string} cacheItemId
 *     The id of the cache element to retrieve. See netmonitor.cache.* localized
 *     strings.
 *
 * @returns {string}
 *     The value corresponding to the provided id.
 */
function getCacheDetailsValue(doc, cacheItemId) {
  const container = doc.getElementById("/Cache/" + cacheItemId);
  ok(!!container, `Found the cache panel container for id ${cacheItemId}`);
  const valueContainer = container.querySelector(".treeValueCell span");
  ok(
    !!valueContainer,
    `Found the cache panel value container for id ${cacheItemId}`
  );

  // The values have opening and closing quotes, remove them using substring.
  // `"some value"` -> `some value`
  const quotedValue = valueContainer.textContent;
  return quotedValue.substring(1, quotedValue.length - 1);
}
