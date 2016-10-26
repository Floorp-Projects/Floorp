/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "data:text/html;charset=utf-8,";

// Test global exit button
addRDMTask(TEST_URL, function* (...args) {
  yield testExitButton(...args);
});

// Test global exit button on detached tab.
// See Bug 1262806
add_task(function* () {
  let tab = yield addTab(TEST_URL);
  let { ui, manager } = yield openRDM(tab);

  yield waitBootstrap(ui);

  let waitTabIsDetached = Promise.all([
    once(tab, "TabClose"),
    once(tab.linkedBrowser, "SwapDocShells")
  ]);

  // Detach the tab with RDM open.
  let newWindow = gBrowser.replaceTabWithWindow(tab);

  // Waiting the tab is detached.
  yield waitTabIsDetached;

  // Get the new tab instance.
  tab = newWindow.gBrowser.tabs[0];

  // Detaching a tab closes RDM.
  ok(!manager.isActiveForTab(tab),
    "Responsive Design Mode is not active for the tab");

  // Reopen the RDM and test the exit button again.
  yield testExitButton(yield openRDM(tab));
  yield BrowserTestUtils.closeWindow(newWindow);
});

function* waitBootstrap(ui) {
  let { toolWindow, tab } = ui;
  let { store } = toolWindow;
  let url = String(tab.linkedBrowser.currentURI.spec);

  // Wait until the viewport has been added.
  yield waitUntilState(store, state => state.viewports.length == 1);

  // Wait until the document has been loaded.
  yield waitForFrameLoad(ui, url);
}

function* testExitButton({ui, manager}) {
  yield waitBootstrap(ui);

  let exitButton = ui.toolWindow.document.getElementById("global-exit-button");

  ok(manager.isActiveForTab(ui.tab),
    "Responsive Design Mode active for the tab");

  exitButton.click();

  yield once(manager, "off");

  ok(!manager.isActiveForTab(ui.tab),
    "Responsive Design Mode is not active for the tab");
}
