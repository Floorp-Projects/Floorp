/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the toolbox keybindings still work after the host is changed.

const URL = "data:text/html;charset=utf8,test page";

var {Toolbox} = require("devtools/client/framework/toolbox");

add_task(function*() {
  info("Create a test tab and open the toolbox");
  let tab = yield addTab(URL);
  let target = TargetFactory.forTab(tab);
  let toolbox = yield gDevTools.showToolbox(target, "webconsole");

  let {SIDE, BOTTOM} = Toolbox.HostType;
  for (let type of [SIDE, BOTTOM, SIDE]) {
    info("Switch to host type " + type);
    yield toolbox.switchHost(type);

    info("Try to use the toolbox shortcuts");
    yield checkKeyBindings(toolbox);
  }

  Services.prefs.clearUserPref("devtools.toolbox.zoomValue");
  Services.prefs.setCharPref("devtools.toolbox.host", BOTTOM);
  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
});

function zoomWithKey(toolbox, key) {
  if (!key) {
    info("Key was empty, skipping zoomWithKey");
    return;
  }

  info("Zooming with key: " + key);
  let currentZoom = toolbox.zoomValue;
  EventUtils.synthesizeKey(key, {accelKey: true}, toolbox.doc.defaultView);
  isnot(toolbox.zoomValue, currentZoom, "The zoom level was changed in the toolbox");
}

function* checkKeyBindings(toolbox) {
  zoomWithKey(toolbox, toolbox.doc.getElementById("toolbox-zoom-in-key").getAttribute("key"));
  zoomWithKey(toolbox, toolbox.doc.getElementById("toolbox-zoom-in-key2").getAttribute("key"));
  zoomWithKey(toolbox, toolbox.doc.getElementById("toolbox-zoom-in-key3").getAttribute("key"));

  zoomWithKey(toolbox, toolbox.doc.getElementById("toolbox-zoom-reset-key").getAttribute("key"));

  zoomWithKey(toolbox, toolbox.doc.getElementById("toolbox-zoom-out-key").getAttribute("key"));
  zoomWithKey(toolbox, toolbox.doc.getElementById("toolbox-zoom-out-key2").getAttribute("key"));

  zoomWithKey(toolbox, toolbox.doc.getElementById("toolbox-zoom-reset-key2").getAttribute("key"));
}
