/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Toolbox } = require("resource://devtools/client/framework/toolbox.js");
const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);

add_task(async function () {
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("devtools.toolbox.zoomValue");
  });

  // This test assume that zoom value will be default value. i.e. x1.0.
  Services.prefs.setCharPref("devtools.toolbox.zoomValue", "1.0");
  await addTab("about:blank");
  const toolbox = await gDevTools.showToolboxForTab(gBrowser.selectedTab, {
    toolId: "styleeditor",
    hostType: Toolbox.HostType.BOTTOM,
  });

  info("testing zoom keys");

  testZoomLevel("In", 2, 1.2, toolbox);
  testZoomLevel("Out", 3, 0.9, toolbox);
  testZoomLevel("Reset", 1, 1, toolbox);

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});

function testZoomLevel(type, times, expected, toolbox) {
  sendZoomKey("toolbox.zoom" + type + ".key", times);

  const zoom = getCurrentZoom(toolbox);
  is(
    zoom.toFixed(1),
    expected.toFixed(1),
    "zoom level correct after zoom " + type
  );

  const savedZoom = parseFloat(
    Services.prefs.getCharPref("devtools.toolbox.zoomValue")
  );
  is(
    savedZoom.toFixed(1),
    expected.toFixed(1),
    "saved zoom level is correct after zoom " + type
  );
}

function sendZoomKey(shortcut, times) {
  for (let i = 0; i < times; i++) {
    synthesizeKeyShortcut(L10N.getStr(shortcut));
  }
}

function getCurrentZoom(toolbox) {
  return toolbox.win.browsingContext.fullZoom;
}
