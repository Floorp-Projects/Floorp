/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {Toolbox} = require("devtools/client/framework/toolbox");
var {SIDE, BOTTOM, WINDOW} = Toolbox.HostType;
var toolbox, target;

const URL = "data:text/html;charset=utf8,test for opening toolbox in different hosts";

add_task(function* runTest() {
  info("Create a test tab and open the toolbox");
  let tab = yield addTab(URL);
  target = TargetFactory.forTab(tab);
  toolbox = yield gDevTools.showToolbox(target, "webconsole");

  yield testBottomHost();
  yield testSidebarHost();
  yield testWindowHost();
  yield testToolSelect();
  yield testDestroy();
  yield testRememberHost();
  yield testPreviousHost();

  yield toolbox.destroy();

  toolbox = target = null;
  gBrowser.removeCurrentTab();
});

function* testBottomHost() {
  checkHostType(toolbox, BOTTOM);

  // test UI presence
  let nbox = gBrowser.getNotificationBox();
  let iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-bottom-iframe");
  ok(iframe, "toolbox bottom iframe exists");

  checkToolboxLoaded(iframe);
}

function* testSidebarHost() {
  yield toolbox.switchHost(SIDE);
  checkHostType(toolbox, SIDE);

  // test UI presence
  let nbox = gBrowser.getNotificationBox();
  let bottom = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-bottom-iframe");
  ok(!bottom, "toolbox bottom iframe doesn't exist");

  let iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-side-iframe");
  ok(iframe, "toolbox side iframe exists");

  checkToolboxLoaded(iframe);
}

function* testWindowHost() {
  yield toolbox.switchHost(WINDOW);
  checkHostType(toolbox, WINDOW);

  let nbox = gBrowser.getNotificationBox();
  let sidebar = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-side-iframe");
  ok(!sidebar, "toolbox sidebar iframe doesn't exist");

  let win = Services.wm.getMostRecentWindow("devtools:toolbox");
  ok(win, "toolbox separate window exists");

  let iframe = win.document.getElementById("toolbox-iframe");
  checkToolboxLoaded(iframe);
}

function* testToolSelect() {
  // make sure we can load a tool after switching hosts
  yield toolbox.selectTool("inspector");
}

function* testDestroy() {
  yield toolbox.destroy();
  target = TargetFactory.forTab(gBrowser.selectedTab);
  toolbox = yield gDevTools.showToolbox(target);
}

function* testRememberHost() {
  // last host was the window - make sure it's the same when re-opening
  is(toolbox.hostType, WINDOW, "host remembered");

  let win = Services.wm.getMostRecentWindow("devtools:toolbox");
  ok(win, "toolbox separate window exists");
}

function* testPreviousHost() {
  // last host was the window - make sure it's the same when re-opening
  is(toolbox.hostType, WINDOW, "host remembered");

  info("Switching to side");
  yield toolbox.switchHost(SIDE);
  checkHostType(toolbox, SIDE, WINDOW);

  info("Switching to bottom");
  yield toolbox.switchHost(BOTTOM);
  checkHostType(toolbox, BOTTOM, SIDE);

  info("Switching from bottom to side");
  yield toolbox.switchToPreviousHost();
  checkHostType(toolbox, SIDE, BOTTOM);

  info("Switching from side to bottom");
  yield toolbox.switchToPreviousHost();
  checkHostType(toolbox, BOTTOM, SIDE);

  info("Switching to window");
  yield toolbox.switchHost(WINDOW);
  checkHostType(toolbox, WINDOW, BOTTOM);

  info("Switching from window to bottom");
  yield toolbox.switchToPreviousHost();
  checkHostType(toolbox, BOTTOM, WINDOW);

  info("Forcing the previous host to match the current (bottom)")
  Services.prefs.setCharPref("devtools.toolbox.previousHost", BOTTOM);

  info("Switching from bottom to side (since previous=current=bottom");
  yield toolbox.switchToPreviousHost();
  checkHostType(toolbox, SIDE, BOTTOM);

  info("Forcing the previous host to match the current (side)")
  Services.prefs.setCharPref("devtools.toolbox.previousHost", SIDE);
  info("Switching from side to bottom (since previous=current=side");
  yield toolbox.switchToPreviousHost();
  checkHostType(toolbox, BOTTOM, SIDE);
}

function checkToolboxLoaded(iframe) {
  let tabs = iframe.contentDocument.getElementById("toolbox-tabs");
  ok(tabs, "toolbox UI has been loaded into iframe");
}
