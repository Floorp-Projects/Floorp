/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  gDevToolsBrowser,
} = require("resource://devtools/client/framework/devtools-browser.js");

const { Toolbox } = require("resource://devtools/client/framework/toolbox.js");
const { LEFT, RIGHT, BOTTOM, WINDOW } = Toolbox.HostType;
let toolbox;

// We are opening/close toolboxes many times,
// which introduces long GC pauses between each sub task
// and requires some more time to run in DEBUG builds.
requestLongerTimeout(2);

const URL =
  "data:text/html;charset=utf8,test for opening toolbox in different hosts";

add_task(async function () {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  win.gBrowser.selectedTab = BrowserTestUtils.addTab(win.gBrowser, URL);

  const tab = win.gBrowser.selectedTab;
  toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "webconsole",
    hostType: Toolbox.HostType.WINDOW,
  });
  const onToolboxClosed = toolbox.once("destroyed");
  ok(
    gDevToolsBrowser.hasToolboxOpened(win),
    "hasToolboxOpened is true before closing the toolbox"
  );
  await BrowserTestUtils.closeWindow(win);
  ok(
    !gDevToolsBrowser.hasToolboxOpened(win),
    "hasToolboxOpened is false after closing the window"
  );

  info("Wait for toolbox to be destroyed after browser window is closed");
  await onToolboxClosed;
  toolbox = null;
});

add_task(async function runTest() {
  info("Create a test tab and open the toolbox");
  const tab = await addTab(URL);
  toolbox = await gDevTools.showToolboxForTab(tab, { toolId: "webconsole" });

  await runHostTests(gBrowser);
  await toolbox.destroy();

  toolbox = null;
  gBrowser.removeCurrentTab();
});

// We run the same host switching tests in a private window.
// See Bug 1581093 for an example of issue specific to private windows.
add_task(async function runPrivateWindowTest() {
  info("Create a private window + tab and open the toolbox");
  await runHostTestsFromSeparateWindow({
    private: true,
  });
});

// We run the same host switching tests in a non-fission window.
// See Bug 1650963 for an example of issue specific to private windows.
add_task(async function runNonFissionWindowTest() {
  info("Create a non-fission window + tab and open the toolbox");
  await runHostTestsFromSeparateWindow({
    fission: false,
  });
});

async function runHostTestsFromSeparateWindow(options) {
  const win = await BrowserTestUtils.openNewBrowserWindow(options);
  const browser = win.gBrowser;
  browser.selectedTab = BrowserTestUtils.addTab(browser, URL);

  const tab = browser.selectedTab;
  toolbox = await gDevTools.showToolboxForTab(tab, { toolId: "webconsole" });

  await runHostTests(browser);
  await toolbox.destroy();

  toolbox = null;
  await BrowserTestUtils.closeWindow(win);
}

async function runHostTests(browser) {
  await testBottomHost(browser);
  await testLeftHost(browser);
  await testRightHost(browser);
  await testWindowHost(browser);
  await testToolSelect();
  await testDestroy(browser);
  await testRememberHost();
  await testPreviousHost();
}

function testBottomHost(browser) {
  checkHostType(toolbox, BOTTOM);

  // test UI presence
  const panel = browser.getPanel();
  const iframe = panel.querySelector(".devtools-toolbox-bottom-iframe");
  ok(iframe, "toolbox bottom iframe exists");

  checkToolboxLoaded(iframe);
}

async function testLeftHost(browser) {
  await toolbox.switchHost(LEFT);
  checkHostType(toolbox, LEFT);

  // test UI presence
  const panel = browser.getPanel();
  const bottom = panel.querySelector(".devtools-toolbox-bottom-iframe");
  ok(!bottom, "toolbox bottom iframe doesn't exist");

  const iframe = panel.querySelector(".devtools-toolbox-side-iframe");
  ok(iframe, "toolbox side iframe exists");

  checkToolboxLoaded(iframe);
}

async function testRightHost(browser) {
  await toolbox.switchHost(RIGHT);
  checkHostType(toolbox, RIGHT);

  // test UI presence
  const panel = browser.getPanel();
  const bottom = panel.querySelector(".devtools-toolbox-bottom-iframe");
  ok(!bottom, "toolbox bottom iframe doesn't exist");

  const iframe = panel.querySelector(".devtools-toolbox-side-iframe");
  ok(iframe, "toolbox side iframe exists");

  checkToolboxLoaded(iframe);
}

async function testWindowHost(browser) {
  await toolbox.switchHost(WINDOW);
  checkHostType(toolbox, WINDOW);

  const panel = browser.getPanel();
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

async function testDestroy(browser) {
  await toolbox.destroy();
  toolbox = await gDevTools.showToolboxForTab(browser.selectedTab);
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
