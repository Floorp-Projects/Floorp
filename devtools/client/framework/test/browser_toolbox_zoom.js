/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {LocalizationHelper} = require("devtools/shared/l10n");
const {Toolbox} = require("devtools/client/framework/toolbox");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

add_task(async function() {
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("devtools.toolbox.zoomValue");
  });

  // This test assume that zoom value will be default value. i.e. x1.0.
  Services.prefs.setCharPref("devtools.toolbox.zoomValue", "1.0");
  await addTab("about:blank");
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = await gDevTools.showToolbox(target,
                                            "styleeditor",
                                            Toolbox.HostType.BOTTOM);

  info("testing zoom keys");

  testZoomLevel("In", 2, 1.2, toolbox);
  testZoomLevel("Out", 3, 0.9, toolbox);
  testZoomLevel("Reset", 1, 1, toolbox);

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});

function testZoomLevel(type, times, expected, toolbox) {
  sendZoomKey("toolbox.zoom" + type + ".key", times);

  let zoom = getCurrentZoom(toolbox);
  is(zoom.toFixed(1), expected, "zoom level correct after zoom " + type);

  let savedZoom = parseFloat(Services.prefs.getCharPref(
    "devtools.toolbox.zoomValue"));
  is(savedZoom.toFixed(1), expected,
     "saved zoom level is correct after zoom " + type);
}

function sendZoomKey(shortcut, times) {
  for (let i = 0; i < times; i++) {
    synthesizeKeyShortcut(L10N.getStr(shortcut));
  }
}

function getCurrentZoom(toolbox) {
  let windowUtils = toolbox.win.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils);
  return windowUtils.fullZoom;
}
