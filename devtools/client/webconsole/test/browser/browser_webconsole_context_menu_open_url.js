/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the Open URL in new Tab menu item is displayed for network logs and works as
// expected.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function() {
  // Enable net messages in the console for this test.
  await pushPref("devtools.webconsole.filter.net", true);

  const hud = await openNewTabAndConsole(TEST_URI);
  hud.ui.clearOutput();

  info("Test Open URL menu item for text log");

  info("Logging a text message in the content window");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.console.log("simple text message");
  });
  let message = await waitFor(() => findMessage(hud, "simple text message"));
  ok(message, "Text log found in the console");

  info("Open and check the context menu for the logged text message");
  let menuPopup = await openContextMenu(hud, message);
  let openUrlItem = menuPopup.querySelector("#console-menu-open-url");
  ok(!openUrlItem, "Open URL menu item is not available");

  await hideContextMenu(hud);
  hud.ui.clearOutput();

  info("Test Open URL menu item for network log");

  info("Reload the content window to produce a network log");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.location.reload();
  });
  message = await waitFor(() => findMessage(hud, "test-console.html"));
  ok(message, "Network log found in the console");

  info("Open and check the context menu for the logged network message");
  menuPopup = await openContextMenu(hud, message);
  openUrlItem = menuPopup.querySelector("#console-menu-open-url");
  ok(openUrlItem, "Open URL menu item is available");

  const currentTab = gBrowser.selectedTab;
  const tabLoaded = listenToTabLoad();
  info("Click on Open URL menu item and wait for new tab to open");
  openUrlItem.click();
  await hideContextMenu(hud);
  const newTab = await tabLoaded;
  const newTabHref = newTab.linkedBrowser.currentURI.spec;
  is(newTabHref, TEST_URI, "Tab was opened with the expected URL");

  info("Remove the new tab and select the previous tab back");
  gBrowser.removeTab(newTab);
  gBrowser.selectedTab = currentTab;
});

/**
 * Simple helper to wrap a tab load listener in a promise.
 */
function listenToTabLoad() {
  return new Promise(resolve => {
    gBrowser.tabContainer.addEventListener(
      "TabOpen",
      function(evt) {
        const newTab = evt.target;
        BrowserTestUtils.browserLoaded(newTab.linkedBrowser).then(() =>
          resolve(newTab)
        );
      },
      { capture: true, once: true }
    );
  });
}
