/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {Toolbox} = require("devtools/client/framework/toolbox");
var {LEFT, RIGHT, BOTTOM, WINDOW} = Toolbox.HostType;
var toolbox, target;

const URL = "data:text/html;charset=utf8,test for opening toolbox in different hosts";

add_task(async function runTest() {
  info("Create a test tab and open the toolbox");
  const tab = await addTab(URL);
  target = await TargetFactory.forTab(tab);
  toolbox = await gDevTools.showToolbox(target, "webconsole");

  await testBottomHost();
  await testLeftHost();
  await testRightHost();
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
  const panel = gBrowser.getPanel();
  const iframe = panel.querySelector(".devtools-toolbox-bottom-iframe");
  ok(iframe, "toolbox bottom iframe exists");

  checkToolboxLoaded(iframe);
}

async function testLeftHost() {
  await toolbox.switchHost(LEFT);
  checkHostType(toolbox, LEFT);

  // test UI presence
  const panel = gBrowser.getPanel();
  const bottom = panel.querySelector(".devtools-toolbox-bottom-iframe");
  ok(!bottom, "toolbox bottom iframe doesn't exist");

  const iframe = panel.querySelector(".devtools-toolbox-side-iframe");
  ok(iframe, "toolbox side iframe exists");

  checkToolboxLoaded(iframe);
}

async function testRightHost() {
  await toolbox.switchHost(RIGHT);
  checkHostType(toolbox, RIGHT);

  // test UI presence
  const panel = gBrowser.getPanel();
  const bottom = panel.querySelector(".devtools-toolbox-bottom-iframe");
  ok(!bottom, "toolbox bottom iframe doesn't exist");

  const iframe = panel.querySelector(".devtools-toolbox-side-iframe");
  ok(iframe, "toolbox side iframe exists");

  checkToolboxLoaded(iframe);
}

async function testWindowHost() {
  await toolbox.switchHost(WINDOW);
  checkHostType(toolbox, WINDOW);

  const panel = gBrowser.getPanel();
  const sidebar = panel.querySelector(".devtools-toolbox-side-iframe");
  ok(!sidebar, "toolbox sidebar iframe doesn't exist");

  const win = Services.wm.getMostRecentWindow("devtools:toolbox");
  ok(win, "toolbox separate window exists");

  const iframe = win.document.querySelector(".devtools-toolbox-window-iframe");
  checkToolboxLoaded(iframe);
}

async function testToolSelect() {
  // make sure we can load a tool after switching hosts
  await toolbox.selectTool("inspector");
}

async function testDestroy() {
  await toolbox.destroy();
  target = await TargetFactory.forTab(gBrowser.selectedTab);
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

  info("Switching to left");
  await toolbox.switchHost(LEFT);
  checkHostType(toolbox, LEFT, WINDOW);

  info("Switching to right");
  await toolbox.switchHost(RIGHT);
  checkHostType(toolbox, RIGHT, LEFT);

  info("Switching to bottom");
  await toolbox.switchHost(BOTTOM);
  checkHostType(toolbox, BOTTOM, RIGHT);

  info("Switching from bottom to right");
  await toolbox.switchToPreviousHost();
  checkHostType(toolbox, RIGHT, BOTTOM);

  info("Switching from right to bottom");
  await toolbox.switchToPreviousHost();
  checkHostType(toolbox, BOTTOM, RIGHT);

  info("Switching to window");
  await toolbox.switchHost(WINDOW);
  checkHostType(toolbox, WINDOW, BOTTOM);

  info("Switching from window to bottom");
  await toolbox.switchToPreviousHost();
  checkHostType(toolbox, BOTTOM, WINDOW);

  info("Forcing the previous host to match the current (bottom)");
  Services.prefs.setCharPref("devtools.toolbox.previousHost", BOTTOM);

  info("Switching from bottom to right (since previous=current=bottom");
  await toolbox.switchToPreviousHost();
  checkHostType(toolbox, RIGHT, BOTTOM);

  info("Forcing the previous host to match the current (right)");
  Services.prefs.setCharPref("devtools.toolbox.previousHost", RIGHT);
  info("Switching from right to bottom (since previous=current=side");
  await toolbox.switchToPreviousHost();
  checkHostType(toolbox, BOTTOM, RIGHT);
}

function checkToolboxLoaded(iframe) {
  const tabs = iframe.contentDocument.querySelector(".toolbox-tabs");
  ok(tabs, "toolbox UI has been loaded into iframe");
}
