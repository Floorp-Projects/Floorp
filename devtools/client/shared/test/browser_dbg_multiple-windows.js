/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Make sure that the debugger attaches to the right tab when multiple windows
 * are open.
 */

var { DevToolsServer } = require("devtools/server/devtools-server");
var { DevToolsClient } = require("devtools/client/devtools-client");

const TAB1_URL = "data:text/html;charset=utf-8,first-tab";
const TAB2_URL = "data:text/html;charset=utf-8,second-tab";

add_task(async function() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  const [type] = await client.connect();
  is(type, "browser", "Root actor should identify itself as a browser.");

  const tab = await addTab(TAB1_URL);
  await testFirstTab(client, tab);
  const win = await addWindow(TAB2_URL);
  await testNewWindow(client, win);
  testFocusFirst(client);
  await testRemoveTab(client, win, tab);
  await client.close();
});

async function testFirstTab(client, tab) {
  ok(!!tab, "Second tab created.");

  const tabs = await client.mainRoot.listTabs();
  const targetFront = tabs.find(grip => grip.url == TAB1_URL);
  ok(targetFront, "Should find a target actor for the first tab.");

  ok(!tabs[0].selected, "The previously opened tab isn't selected.");
  ok(tabs[1].selected, "The first tab is selected.");
}

async function testNewWindow(client, win) {
  ok(!!win, "Second window created.");

  win.focus();

  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  is(topWindow, win, "The second window is on top.");

  if (Services.focus.activeWindow != win) {
    await new Promise(resolve => {
      win.addEventListener(
        "activate",
        function onActivate(event) {
          if (event.target != win) {
            return;
          }
          win.removeEventListener("activate", onActivate, true);
          resolve();
        },
        true
      );
    });
  }

  const tabs = await client.mainRoot.listTabs();
  ok(!tabs[0].selected, "The previously opened tab isn't selected.");
  ok(!tabs[1].selected, "The first tab isn't selected.");
  ok(tabs[2].selected, "The second tab is selected.");
}

async function testFocusFirst(client) {
  const tab = window.gBrowser.selectedTab;
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    const onFocus = new Promise(resolve => {
      content.addEventListener("focus", resolve, { once: true });
    });
    await onFocus;
  });

  const tabs = await client.mainRoot.listTabs();
  ok(!tabs[0].selected, "The previously opened tab isn't selected.");
  ok(!tabs[1].selected, "The first tab is selected after focusing on i.");
  ok(tabs[2].selected, "The second tab isn't selected.");
}

async function testRemoveTab(client, win, tab) {
  win.close();

  // give it time to close
  await new Promise(resolve => executeSoon(resolve));
  await continue_remove_tab(client, tab);
}

async function continue_remove_tab(client, tab) {
  removeTab(tab);

  const tabs = await client.mainRoot.listTabs();
  // Verify that tabs are no longer included in listTabs.
  const foundTab1 = tabs.some(grip => grip.url == TAB1_URL);
  const foundTab2 = tabs.some(grip => grip.url == TAB2_URL);
  ok(!foundTab1, "Tab1 should be gone.");
  ok(!foundTab2, "Tab2 should be gone.");

  ok(tabs[0].selected, "The previously opened tab is selected.");
}

async function addWindow(url) {
  info("Adding window: " + url);
  const onNewWindow = BrowserTestUtils.waitForNewWindow({ url });
  window.open(url, "_blank", "noopener");
  return onNewWindow;
}
