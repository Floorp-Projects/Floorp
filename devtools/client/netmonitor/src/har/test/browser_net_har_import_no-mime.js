/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests importing HAR with missing `response.content.mimeType` does not crash the netmonitor.
 */
add_task(async () => {
  const { monitor } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });

  info("Starting test... ");

  const { document, actions, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  const { HarImporter } = windowRequire(
    "devtools/client/netmonitor/src/har/har-importer"
  );

  store.dispatch(Actions.batchEnable(false));

  // Invalid HAR json which should contain `entries[0].response.content.mimeType`
  const invalidHarJSON = {
    log: {
      version: "1.2",
      pages: [
        {
          title: "bla",
        },
      ],
      entries: [
        {
          request: {
            method: "POST",
            url: "https://bla.com",
            httpVersion: "",
            headers: [],
            cookies: [],
            queryString: [],
          },
          response: {
            content: {
              size: 1231,
              text: '{"requests":[{"uri":"https://bla.com"}]}',
            },
            headers: [],
          },
          timings: {},
          cache: {},
        },
      ],
    },
  };

  // Import invalid Har file
  const importer = new HarImporter(actions);
  importer.import(JSON.stringify(invalidHarJSON));

  const waitForResponsePanelOpen = waitUntil(() =>
    document.querySelector("#response-panel")
  );

  // Open the response details panel
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelector(".request-list-item")
  );
  clickOnSidebarTab(document, "response");

  await waitForResponsePanelOpen;
  ok(true, "The response panel opened");

  // Clean up
  return teardown(monitor);
});
