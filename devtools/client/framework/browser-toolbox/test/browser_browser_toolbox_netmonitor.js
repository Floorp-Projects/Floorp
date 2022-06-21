/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global gToolbox */

add_task(async function() {
  Services.prefs.setCharPref(
    "devtools.netmonitor.visibleColumns",
    JSON.stringify([
      "domain",
      "file",
      "url",
      "method",
      "status",
      "type",
      "waterfall",
    ])
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("devtools.netmonitor.visibleColumns");
  });

  const ToolboxTask = await initBrowserToolboxTask({
    enableBrowserToolboxFission: true,
  });

  await ToolboxTask.importFunctions({
    waitUntil,
  });

  await ToolboxTask.spawn(null, async () => {
    await gToolbox.selectTool("netmonitor");
    const monitor = gToolbox.getCurrentPanel();
    const { document, store, windowRequire } = monitor.panelWin;

    const Actions = windowRequire(
      "devtools/client/netmonitor/src/actions/index"
    );

    store.dispatch(Actions.batchEnable(false));

    ok(
      !!document.querySelector(".request-list-empty-notice"),
      "An empty notice should be displayed when the frontend is opened."
    );
    is(
      store.getState().requests.requests.length,
      0,
      "The requests should be empty when the frontend is opened."
    );
  });

  info("Trigger request in parent process and check that it shows up");
  await fetch("https://example.org/document-builder.sjs?html=fromParent");

  await ToolboxTask.spawn(null, async () => {
    const monitor = gToolbox.getCurrentPanel();
    const { document, store } = monitor.panelWin;

    await waitUntil(
      () => !document.querySelector(".request-list-empty-notice")
    );
    ok(true, "The empty notice is no longer displayed");
    is(
      store.getState().requests.requests.length,
      1,
      "There's 1 request in the store"
    );

    const requests = Array.from(
      document.querySelectorAll("tbody .requests-list-column.requests-list-url")
    );
    is(requests.length, 1, "One request displayed");
    is(
      requests[0].textContent,
      "https://example.org/document-builder.sjs?html=fromParent",
      "Expected request is displayed"
    );
  });

  info("Trigger content process requests");
  const urlImg = `${URL_ROOT_SSL}test-image.png?fromContent&${Date.now()}-${Math.random()}`;
  await addTab(
    `https://example.com/document-builder.sjs?html=${encodeURIComponent(
      `<img src='${urlImg}'>`
    )}`
  );

  await ToolboxTask.spawn(urlImg, async innerUrlImg => {
    const monitor = gToolbox.getCurrentPanel();
    const { document, store } = monitor.panelWin;

    await waitUntil(() => store.getState().requests.requests.length >= 3);
    ok(true, "Expected content requests are displayed");

    const requests = Array.from(
      document.querySelectorAll("tbody .requests-list-column.requests-list-url")
    );
    is(requests.length, 3, "Three requests displayed");
    ok(
      requests[1].textContent.includes(
        `https://example.com/document-builder.sjs`
      ),
      "Request for the tab is displayed"
    );
    is(
      requests[2].textContent,
      innerUrlImg,
      "Request for image image in tab is displayed"
    );
  });
});
