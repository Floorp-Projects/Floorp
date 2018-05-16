/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the toolbox keybindings still work after the host is changed.

const URL = "data:text/html;charset=utf8,test page";

var {Toolbox} = require("devtools/client/framework/toolbox");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

function getZoomValue() {
  return parseFloat(Services.prefs.getCharPref("devtools.toolbox.zoomValue"));
}

add_task(async function() {
  info("Create a test tab and open the toolbox");
  let tab = await addTab(URL);
  let target = TargetFactory.forTab(tab);
  let toolbox = await gDevTools.showToolbox(target, "webconsole");

  let {SIDE, BOTTOM} = Toolbox.HostType;
  for (let type of [SIDE, BOTTOM, SIDE]) {
    info("Switch to host type " + type);
    await toolbox.switchHost(type);

    info("Try to use the toolbox shortcuts");
    await checkKeyBindings(toolbox);
  }

  Services.prefs.clearUserPref("devtools.toolbox.zoomValue");
  Services.prefs.setCharPref("devtools.toolbox.host", BOTTOM);
  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});

function zoomWithKey(toolbox, key) {
  let shortcut = L10N.getStr(key);
  if (!shortcut) {
    info("Key was empty, skipping zoomWithKey");
    return;
  }
  info("Zooming with key: " + key);
  let currentZoom = getZoomValue();
  synthesizeKeyShortcut(shortcut, toolbox.win);
  isnot(getZoomValue(), currentZoom, "The zoom level was changed in the toolbox");
}

function checkKeyBindings(toolbox) {
  zoomWithKey(toolbox, "toolbox.zoomIn.key");
  zoomWithKey(toolbox, "toolbox.zoomIn2.key");

  zoomWithKey(toolbox, "toolbox.zoomReset.key");

  zoomWithKey(toolbox, "toolbox.zoomOut.key");
  zoomWithKey(toolbox, "toolbox.zoomOut2.key");

  zoomWithKey(toolbox, "toolbox.zoomReset2.key");
}
