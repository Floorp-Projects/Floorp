/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global gToolbox */

add_task(async function() {
  // Disable safebrowsing components which can trigger network requests.
  Services.prefs.setBoolPref("browser.safebrowsing.blockedURIs.enabled", false);
  Services.prefs.setBoolPref("browser.safebrowsing.downloads.enabled", false);
  Services.prefs.setBoolPref("browser.safebrowsing.malware.enabled", false);
  Services.prefs.setBoolPref("browser.safebrowsing.passwords.enabled", false);
  Services.prefs.setBoolPref("browser.safebrowsing.phishing.enabled", false);
  // We can't disable RemoteSettings, but assigning it an erroneous URL will prevent
  // the request to show up in the Netmonitor.
  Services.prefs.setCharPref("services.settings.server", "invalid::/err");
  // Define a set list of visible columns
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
    Services.prefs.clearUserPref("browser.safebrowsing.blockedURIs.enabled");
    Services.prefs.clearUserPref("browser.safebrowsing.downloads.enabled");
    Services.prefs.clearUserPref("browser.safebrowsing.malware.enabled");
    Services.prefs.clearUserPref("browser.safebrowsing.passwords.enabled");
    Services.prefs.clearUserPref("browser.safebrowsing.phishing.enabled");
    Services.prefs.clearUserPref("services.settings.server");
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

    await waitUntil(
      () => !!document.querySelector(".request-list-empty-notice")
    );

    const emptyListNotice = document.querySelector(
      ".request-list-empty-notice"
    );

    ok(
      !!emptyListNotice,
      "An empty notice should be displayed when the frontend is opened."
    );

    is(
      emptyListNotice.innerText,
      "Perform a request to see detailed information about network activity.",
      "The reload and perfomance analysis details should not be visible in the browser toolbox"
    );

    is(
      store.getState().requests.requests.length,
      0,
      "The requests should be empty when the frontend is opened."
    );

    ok(
      !document.querySelector(".requests-list-network-summary-button"),
      "The perfomance analysis button should not be visible in the browser toolbox"
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
