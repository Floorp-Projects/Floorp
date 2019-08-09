// Test that URL opens in a new tab when click while
// pressing CTR (or CMD in MacOS) as expected.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function() {
  // Enable net messages in the console for this test.
  await pushPref("devtools.webconsole.filter.net", true);
  const isMacOS = Services.appinfo.OS === "Darwin";

  // We open the console
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Reload the content window to produce a network log");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.location.reload();
  });
  const message = await waitFor(() => findMessage(hud, "test-console.html"));
  ok(message, "Network log found in the console");

  const currentTab = gBrowser.selectedTab;
  const tabLoaded = listenToTabLoad();

  info("Cmd/Ctrl click on the message");
  const urlObject = message.querySelector(".url");

  EventUtils.sendMouseEvent(
    {
      type: "click",
      [isMacOS ? "metaKey" : "ctrlKey"]: true,
    },
    urlObject,
    hud.ui.window
  );

  info("Opening the URL of the message on a new tab");
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
