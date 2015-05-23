/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the toolbox keybindings still work after the host is changed.

const URL = "data:text/html;charset=utf8,test page";

add_task(function*() {
  info("Create a test tab and open the toolbox");
  let tab = yield addTab(URL);
  let target = TargetFactory.forTab(tab);
  let toolbox = yield gDevTools.showToolbox(target, "webconsole");

  let {SIDE, BOTTOM} = devtools.Toolbox.HostType;
  for (let type of [SIDE, BOTTOM, SIDE]) {
    info("Switch to host type " + type);
    yield toolbox.switchHost(type);

    info("Try to use the toolbox shortcuts");
    yield checkKeyBindings(toolbox);
  }

  Services.prefs.clearUserPref("devtools.toolbox.zoomValue", BOTTOM);
  Services.prefs.setCharPref("devtools.toolbox.host", BOTTOM);
  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
});

function* checkKeyBindings(toolbox) {
  let currentZoom = toolbox.zoomValue;

  let key = toolbox.doc.getElementById("toolbox-zoom-in-key").getAttribute("key");
  EventUtils.synthesizeKey(key, {accelKey: true}, toolbox.doc.defaultView);

  let newZoom = toolbox.zoomValue;
  isnot(newZoom, currentZoom, "The zoom level was changed in the toolbox");
}
