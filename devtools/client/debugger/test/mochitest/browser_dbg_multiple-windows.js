/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the debugger attaches to the right tab when multiple windows
 * are open.
 */

const TAB1_URL = EXAMPLE_URL + "doc_script-switching-01.html";
const TAB2_URL = EXAMPLE_URL + "doc_script-switching-02.html";

add_task(async function() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);
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

  const response = await client.listTabs();
  const targetActor = response.tabs.filter(grip => grip.url == TAB1_URL).pop();
  ok(targetActor, "Should find a target actor for the first tab.");

  is(response.selected, 1, "The first tab is selected.");
}

async function testNewWindow(client, win) {
  ok(!!win, "Second window created.");

  win.focus();

  const topWindow = Services.wm.getMostRecentWindow("navigator:browser");
  is(topWindow, win, "The second window is on top.");

  const isActive = promise.defer();
  const isLoaded = promise.defer();

  if (Services.focus.activeWindow != win) {
    win.addEventListener("activate", function onActivate(event) {
      if (event.target != win) {
        return;
      }
      win.removeEventListener("activate", onActivate, true);
      isActive.resolve();
    }, true);
  } else {
    isActive.resolve();
  }

  const contentLocation = win.content.location.href;
  if (contentLocation != TAB2_URL) {
    win.document.addEventListener("load", function onLoad(event) {
      if (event.target.documentURI != TAB2_URL) {
        return;
      }
      win.document.removeEventListener("load", onLoad, true);
      isLoaded.resolve();
    }, true);
  } else {
    isLoaded.resolve();
  }

  await isActive.promise;
  await isLoaded.promise;
  const response = await client.listTabs();
  is(response.selected, 2, "The second tab is selected.");
}

async function testFocusFirst(client) {
  const onFocus = once(window.content, "focus");

  window.content.focus();
  await onFocus;

  const response = await client.listTabs();
  is(response.selected, 1, "The first tab is selected after focusing on it.");
}

async function testRemoveTab(client, win, tab) {
  win.close();

  // give it time to close
  await new Promise(resolve => executeSoon(resolve));
  await continue_remove_tab(client, tab);
}

async function continue_remove_tab(client, tab)
{
  removeTab(tab);

  const response = await client.listTabs();
  // Verify that tabs are no longer included in listTabs.
  let foundTab1 = response.tabs.some(grip => grip.url == TAB1_URL);
  let foundTab2 = response.tabs.some(grip => grip.url == TAB2_URL);
  ok(!foundTab1, "Tab1 should be gone.");
  ok(!foundTab2, "Tab2 should be gone.");

  is(response.selected, 0, "The original tab is selected.");
}
