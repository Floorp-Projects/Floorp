/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if font preview is generated correctly
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(FONTS_URL + "?name=fonts", {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Reload the page to get the font request
  const waitForRequests = waitForNetworkEvents(monitor, 3);
  tab.linkedBrowser.reload();
  await waitForRequests;

  const wait = waitForDOMIfNeeded(
    document,
    "#response-panel .response-font[src^='data:']"
  );

  const requests = document.querySelectorAll(".request-list-item");

  // Check first font request
  clickElement(requests[1], monitor);
  clickOnSidebarTab(document, "response");

  await wait;

  ok(true, "Font preview is shown");

  const tabpanel = document.querySelector("#response-panel");
  let image = tabpanel.querySelector(".response-font");
  await once(image, "load");

  ok(
    image.complete && image.naturalHeight !== 0,
    "Font preview got generated correctly"
  );

  let fontData = document.querySelectorAll(".tabpanel-summary-value");
  is(fontData[0].textContent, "Zilla Slab", "Font name is correct");
  is(
    fontData[1].textContent,
    "application/octet-stream",
    "MIME type is correct"
  );

  // Check second font request
  clickElement(requests[2], monitor);

  await waitForDOM(document, "#response-panel .response-font[src^='data:']");

  image = tabpanel.querySelector(".response-font");
  await once(image, "load");

  ok(
    image.complete && image.naturalHeight !== 0,
    "Font preview got generated correctly"
  );

  fontData = document.querySelectorAll(".tabpanel-summary-value");
  is(fontData[0].textContent, "Zilla Slab Bold", "Font name is correct");
  is(
    fontData[1].textContent,
    "application/octet-stream",
    "MIME type is correct"
  );

  await teardown(monitor);
});
