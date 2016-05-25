/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the toolbox keybindings still work after the host is changed.

const URL = "data:text/html;charset=utf8,test page";

var {Toolbox} = require("devtools/client/framework/toolbox");
var strings = Services.strings.createBundle(
  "chrome://devtools/locale/toolbox.properties");

add_task(function* () {
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
  let shortcut = strings.GetStringFromName(key);
  if (!shortcut) {
    info("Key was empty, skipping zoomWithKey");
    return;
  }
  info("Zooming with key: " + key);
  let currentZoom = toolbox.zoomValue;
  synthesizeKeyShortcut(shortcut);
  isnot(toolbox.zoomValue, currentZoom, "The zoom level was changed in the toolbox");
}

function* checkKeyBindings(toolbox) {
  zoomWithKey(toolbox, "toolbox.zoomIn.key");
  zoomWithKey(toolbox, "toolbox.zoomIn2.key");
  zoomWithKey(toolbox, "toolbox.zoomIn3.key");

  zoomWithKey(toolbox, "toolbox.zoomReset.key");

  zoomWithKey(toolbox, "toolbox.zoomOut.key");
  zoomWithKey(toolbox, "toolbox.zoomOut2.key");

  zoomWithKey(toolbox, "toolbox.zoomReset2.key");
}
