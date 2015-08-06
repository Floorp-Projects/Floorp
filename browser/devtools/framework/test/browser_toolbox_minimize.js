/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when the toolbox is displayed in a bottom host, that host can be
// minimized to just the tabbar height, and maximized again.
// Also test that while minimized, switching to a tool, clicking on the
// settings, or clicking on the selected tool's tab maximizes the toolbox again.
// Finally test that the minimize button doesn't exist in other host types.

const URL = "data:text/html;charset=utf8,test page";
const {Toolbox} = require("devtools/framework/toolbox");

add_task(function*() {
  info("Create a test tab and open the toolbox");
  let tab = yield addTab(URL);
  let target = TargetFactory.forTab(tab);
  let toolbox = yield gDevTools.showToolbox(target, "webconsole");

  let button = toolbox.doc.querySelector("#toolbox-dock-bottom-minimize");
  ok(button, "The minimize button exists in the default bottom host");

  info("Try to minimize the toolbox");
  yield minimize(toolbox);
  ok(parseInt(toolbox._host.frame.style.marginBottom, 10) < 0,
     "The toolbox host has been hidden away with a negative-margin");

  info("Try to maximize again the toolbox");
  yield maximize(toolbox);
  ok(parseInt(toolbox._host.frame.style.marginBottom, 10) == 0,
     "The toolbox host is shown again");

  info("Try to minimize again using the keyboard shortcut");
  yield minimizeWithShortcut(toolbox);
  ok(parseInt(toolbox._host.frame.style.marginBottom, 10) < 0,
     "The toolbox host has been hidden away with a negative-margin");

  info("Try to maximize again using the keyboard shortcut");
  yield maximizeWithShortcut(toolbox);
  ok(parseInt(toolbox._host.frame.style.marginBottom, 10) == 0,
     "The toolbox host is shown again");

  info("Minimize again and switch to another tool");
  yield minimize(toolbox);
  let onMaximized = toolbox._host.once("maximized");
  yield toolbox.selectTool("inspector");
  yield onMaximized;

  info("Minimize again and click on the tab of the current tool");
  yield minimize(toolbox);
  onMaximized = toolbox._host.once("maximized");
  let tabButton = toolbox.doc.querySelector("#toolbox-tab-inspector");
  EventUtils.synthesizeMouseAtCenter(tabButton, {}, toolbox.doc.defaultView);
  yield onMaximized;

  info("Minimize again and click on the settings tab");
  yield minimize(toolbox);
  onMaximized = toolbox._host.once("maximized");
  let settingsButton = toolbox.doc.querySelector("#toolbox-tab-options");
  EventUtils.synthesizeMouseAtCenter(settingsButton, {}, toolbox.doc.defaultView);
  yield onMaximized;

  info("Switch to a different host");
  yield toolbox.switchHost(Toolbox.HostType.SIDE);
  button = toolbox.doc.querySelector("#toolbox-dock-bottom-minimize");
  ok(!button, "The minimize button doesn't exist in the side host");

  Services.prefs.clearUserPref("devtools.toolbox.host");
  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
});

function* minimize(toolbox) {
  let button = toolbox.doc.querySelector("#toolbox-dock-bottom-minimize");
  let onMinimized = toolbox._host.once("minimized");
  EventUtils.synthesizeMouseAtCenter(button, {}, toolbox.doc.defaultView);
  yield onMinimized;
}

function* minimizeWithShortcut(toolbox) {
  let key = toolbox.doc.getElementById("toolbox-minimize-key")
                       .getAttribute("key");
  let onMinimized = toolbox._host.once("minimized");
  EventUtils.synthesizeKey(key, {accelKey: true, shiftKey: true},
                           toolbox.doc.defaultView);
  yield onMinimized;
}

function* maximize(toolbox) {
  let button = toolbox.doc.querySelector("#toolbox-dock-bottom-minimize");
  let onMaximized = toolbox._host.once("maximized");
  EventUtils.synthesizeMouseAtCenter(button, {}, toolbox.doc.defaultView);
  yield onMaximized;
}

function* maximizeWithShortcut(toolbox) {
  let key = toolbox.doc.getElementById("toolbox-minimize-key")
                       .getAttribute("key");
  let onMaximized = toolbox._host.once("maximized");
  EventUtils.synthesizeKey(key, {accelKey: true, shiftKey: true},
                           toolbox.doc.defaultView);
  yield onMaximized;
}
