/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the Open URL in new Tab menu item is displayed for links in text messages
// and network logs and that they work as expected.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";
const TEST_URI2 = "http://example.com/";

add_task(async function () {
  // Enable net messages in the console for this test.
  await pushPref("devtools.webconsole.filter.net", true);

  const hud = await openNewTabAndConsole(TEST_URI);
  await clearOutput(hud);

  info("Test Open URL menu item for text log");

  info("Logging a text message in the content window");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.console.log("simple text message");
  });
  let message = await waitFor(() =>
    findConsoleAPIMessage(hud, "simple text message")
  );
  ok(message, "Text log found in the console");

  info("Open and check the context menu for the logged text message");
  let menuPopup = await openContextMenu(hud, message);
  let openUrlItem = menuPopup.querySelector("#console-menu-open-url");
  ok(!openUrlItem, "Open URL menu item is not available");

  await hideContextMenu(hud);

  info("Test Open URL menu item for a text message containing a link");
  await ContentTask.spawn(gBrowser.selectedBrowser, TEST_URI2, url => {
    content.wrappedJSObject.console.log("Visit", url);
  });

  info("Open context menu for the link");
  message = await waitFor(() => findConsoleAPIMessage(hud, "Visit"));
  const urlNode = message.querySelector("a.url");
  menuPopup = await openContextMenu(hud, urlNode);
  openUrlItem = menuPopup.querySelector("#console-menu-open-url");
  ok(openUrlItem, "Open URL menu item is available");

  info("Click on Open URL menu item and wait for new tab to open");
  let currentTab = gBrowser.selectedTab;
  const onTabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, TEST_URI2, true);
  openUrlItem.click();
  let newTab = await onTabLoaded;
  ok(newTab, "The expected tab was opened.");
  is(
    newTab._tPos,
    currentTab._tPos + 1,
    "The new tab was opened in the position to the right of the current tab"
  );
  is(gBrowser.selectedTab, currentTab, "The tab was opened in the background");

  await clearOutput(hud);

  info("Test Open URL menu item for network log");

  info("Reload the content window to produce a network log");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.location.reload();
  });
  message = await waitFor(() => findNetworkMessage(hud, "test-console.html"));
  ok(message, "Network log found in the console");

  info("Open and check the context menu for the logged network message");
  menuPopup = await openContextMenu(hud, message);
  openUrlItem = menuPopup.querySelector("#console-menu-open-url");
  ok(openUrlItem, "Open URL menu item is available");

  currentTab = gBrowser.selectedTab;
  const tabLoaded = listenToTabLoad();
  info("Click on Open URL menu item and wait for new tab to open");
  openUrlItem.click();
  await hideContextMenu(hud);
  newTab = await tabLoaded;
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
      function (evt) {
        const newTab = evt.target;
        BrowserTestUtils.browserLoaded(newTab.linkedBrowser).then(() =>
          resolve(newTab)
        );
      },
      { capture: true, once: true }
    );
  });
}
