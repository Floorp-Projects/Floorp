/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "data:text/html;charset=utf-8,";

// Test global exit button
addRDMTask(TEST_URL, async function(...args) {
  await testExitButton(...args);
});

// Test global exit button on detached tab.
// See Bug 1262806
add_task(async function() {
  let tab = await addTab(TEST_URL);
  const { ui, manager } = await openRDM(tab);

  await waitBootstrap(ui);

  const waitTabIsDetached = Promise.all([
    once(tab, "TabClose"),
    once(tab.linkedBrowser, "SwapDocShells")
  ]);

  // Detach the tab with RDM open.
  const newWindow = gBrowser.replaceTabWithWindow(tab);

  // Wait until the tab is detached and the new window is fully initialized.
  await waitTabIsDetached;
  await newWindow.delayedStartupPromise;

  // Get the new tab instance.
  tab = newWindow.gBrowser.tabs[0];

  // Detaching a tab closes RDM.
  ok(!manager.isActiveForTab(tab),
    "Responsive Design Mode is not active for the tab");

  // Reopen the RDM and test the exit button again.
  await testExitButton(await openRDM(tab));
  await BrowserTestUtils.closeWindow(newWindow);
});

async function waitBootstrap(ui) {
  const { toolWindow, tab } = ui;
  const { store } = toolWindow;
  const url = String(tab.linkedBrowser.currentURI.spec);

  // Wait until the viewport has been added.
  await waitUntilState(store, state => state.viewports.length == 1);

  // Wait until the document has been loaded.
  await waitForFrameLoad(ui, url);
}

async function testExitButton({ui, manager}) {
  await waitBootstrap(ui);

  const exitButton = ui.toolWindow.document.getElementById("global-exit-button");

  ok(manager.isActiveForTab(ui.tab),
    "Responsive Design Mode active for the tab");

  exitButton.click();

  await once(manager, "off");

  ok(!manager.isActiveForTab(ui.tab),
    "Responsive Design Mode is not active for the tab");
}
