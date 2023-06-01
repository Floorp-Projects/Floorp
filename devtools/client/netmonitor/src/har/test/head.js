/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */
/* import-globals-from ../../../test/head.js */

// Load the NetMonitor head.js file to share its API.
var netMonitorHead =
  "chrome://mochitests/content/browser/devtools/client/netmonitor/test/head.js";
Services.scriptloader.loadSubScript(netMonitorHead, this);

// Directory with HAR related test files.
const HAR_EXAMPLE_URL =
  "http://example.com/browser/devtools/client/netmonitor/src/har/test/";

/**
 * Trigger a "copy all as har" from the context menu of the requests list.

 * @param {Object} monitor
 *        The network monitor object
 */
async function copyAllAsHARWithContextMenu(monitor, { asString = false } = {}) {
  const { HarMenuUtils } = monitor.panelWin.windowRequire(
    "devtools/client/netmonitor/src/har/har-menu-utils"
  );

  info("Open the context menu on the first visible request.");
  const firstRequest =
    monitor.panelWin.document.querySelectorAll(".request-list-item")[0];

  EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);
  EventUtils.sendMouseEvent({ type: "contextmenu" }, firstRequest);

  info("Trigger Copy All As HAR from the context menu");
  const onHarCopyDone = HarMenuUtils.once("copy-all-as-har-done");
  await selectContextMenuItem(monitor, "request-list-context-copy-all-as-har");
  const jsonString = await onHarCopyDone;

  if (asString) {
    return jsonString;
  }
  return JSON.parse(jsonString);
}
