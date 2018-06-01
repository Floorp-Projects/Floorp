/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {Toolbox} = require("devtools/client/framework/toolbox");
var {SIDE, BOTTOM, WINDOW} = Toolbox.HostType;
var toolbox, target;

const URL = "data:text/html;charset=utf8,test for opening toolbox in different hosts";

add_task(async function runTest() {
  info("Create a test tab and open the toolbox");
  const tab = await addTab(URL);
  target = TargetFactory.forTab(tab);
  toolbox = await gDevTools.showToolbox(target, "webconsole");

  await testBottomHost();
  await testSidebarHost();
  await testWindowHost();
  await testToolSelect();
  await testDestroy();
  await testRememberHost();
  await testPreviousHost();

  await toolbox.destroy();

  toolbox = target = null;
  gBrowser.removeCurrentTab();
});

function testBottomHost() {
  checkHostType(toolbox, BOTTOM);

  // test UI presence
  const nbox = gBrowser.getNotificationBox();
  const iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-bottom-iframe");
  ok(iframe, "toolbox bottom iframe exists");

  checkToolboxLoaded(iframe);
}

async function testSidebarHost() {
  await toolbox.switchHost(SIDE);
  checkHostType(toolbox, SIDE);

  // test UI presence
  const nbox = gBrowser.getNotificationBox();
  const bottom = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-bottom-iframe");
  ok(!bottom, "toolbox bottom iframe doesn't exist");

  const iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-side-iframe");
  ok(iframe, "toolbox side iframe exists");

  checkToolboxLoaded(iframe);
}

async function testWindowHost() {
  await toolbox.switchHost(WINDOW);
  checkHostType(toolbox, WINDOW);

  const nbox = gBrowser.getNotificationBox();
  const sidebar = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-side-iframe");
  ok(!sidebar, "toolbox sidebar iframe doesn't exist");

  const win = Services.wm.getMostRecentWindow("devtools:toolbox");
  ok(win, "toolbox separate window exists");

  const iframe = win.document.getElementById("toolbox-iframe");
  checkToolboxLoaded(iframe);
}

async function testToolSelect() {
  // make sure we can load a tool after switching hosts
  await toolbox.selectTool("inspector");
}

async function testDestroy() {
  await toolbox.destroy();
  target = TargetFactory.forTab(gBrowser.selectedTab);
  toolbox = await gDevTools.showToolbox(target);
}

function testRememberHost() {
  // last host was the window - make sure it's the same when re-opening
  is(toolbox.hostType, WINDOW, "host remembered");

  const win = Services.wm.getMostRecentWindow("devtools:toolbox");
  ok(win, "toolbox separate window exists");
}

async function testPreviousHost() {
  // last host was the window - make sure it's the same when re-opening
  is(toolbox.hostType, WINDOW, "host remembered");

  info("Switching to side");
  await toolbox.switchHost(SIDE);
  checkHostType(toolbox, SIDE, WINDOW);

  info("Switching to bottom");
  await toolbox.switchHost(BOTTOM);
  checkHostType(toolbox, BOTTOM, SIDE);

  info("Switching from bottom to side");
  await toolbox.switchToPreviousHost();
  checkHostType(toolbox, SIDE, BOTTOM);

  info("Switching from side to bottom");
  await toolbox.switchToPreviousHost();
  checkHostType(toolbox, BOTTOM, SIDE);

  info("Switching to window");
  await toolbox.switchHost(WINDOW);
  checkHostType(toolbox, WINDOW, BOTTOM);

  info("Switching from window to bottom");
  await toolbox.switchToPreviousHost();
  checkHostType(toolbox, BOTTOM, WINDOW);

  info("Forcing the previous host to match the current (bottom)");
  Services.prefs.setCharPref("devtools.toolbox.previousHost", BOTTOM);

  info("Switching from bottom to side (since previous=current=bottom");
  await toolbox.switchToPreviousHost();
  checkHostType(toolbox, SIDE, BOTTOM);

  info("Forcing the previous host to match the current (side)");
  Services.prefs.setCharPref("devtools.toolbox.previousHost", SIDE);
  info("Switching from side to bottom (since previous=current=side");
  await toolbox.switchToPreviousHost();
  checkHostType(toolbox, BOTTOM, SIDE);
}

function checkToolboxLoaded(iframe) {
  const tabs = iframe.contentDocument.querySelector(".toolbox-tabs");
  ok(tabs, "toolbox UI has been loaded into iframe");
}
