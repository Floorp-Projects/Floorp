"use strict";

add_task(async function test_sessions_tab_value_private() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedWindowCount(),
    0,
    "No closed window sessions at start of test"
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "exampleextension@mozilla.org",
        },
      },
      permissions: ["sessions"],
    },
    background() {
      browser.test.onMessage.addListener(async (msg, pbw) => {
        if (msg == "value") {
          await browser.test.assertRejects(
            browser.sessions.setWindowValue(pbw.windowId, "foo", "bar"),
            /Invalid window ID/,
            "should not be able to set incognito window session data"
          );
          await browser.test.assertRejects(
            browser.sessions.getWindowValue(pbw.windowId, "foo"),
            /Invalid window ID/,
            "should not be able to get incognito window session data"
          );
          await browser.test.assertRejects(
            browser.sessions.removeWindowValue(pbw.windowId, "foo"),
            /Invalid window ID/,
            "should not be able to remove incognito window session data"
          );
          await browser.test.assertRejects(
            browser.sessions.setTabValue(pbw.tabId, "foo", "bar"),
            /Invalid tab ID/,
            "should not be able to set incognito tab session data"
          );
          await browser.test.assertRejects(
            browser.sessions.getTabValue(pbw.tabId, "foo"),
            /Invalid tab ID/,
            "should not be able to get incognito tab session data"
          );
          await browser.test.assertRejects(
            browser.sessions.removeTabValue(pbw.tabId, "foo"),
            /Invalid tab ID/,
            "should not be able to remove incognito tab session data"
          );
        }
        if (msg == "restore") {
          await browser.test.assertRejects(
            browser.sessions.restore(),
            /Could not restore object/,
            "should not be able to restore incognito last window session data"
          );
          if (pbw) {
            await browser.test.assertRejects(
              browser.sessions.restore(pbw.sessionId),
              /Could not restore object/,
              `should not be able to restore incognito session ID ${pbw.sessionId} session data`
            );
          }
        }
        browser.test.sendMessage("done");
      });
    },
  });

  let winData = await getIncognitoWindow("http://mochi.test:8888/");
  await extension.startup();

  // Test value set/get APIs on a private window and tab.
  extension.sendMessage("value", winData.details);
  await extension.awaitMessage("done");

  // Test restoring a private tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    winData.win.gBrowser,
    "http://example.com"
  );
  let sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  BrowserTestUtils.removeTab(tab);
  await sessionUpdatePromise;
  let closedTabData = SessionStore.getClosedTabData(winData.win);

  extension.sendMessage("restore", {
    sessionId: String(closedTabData[0].closedId),
  });
  await extension.awaitMessage("done");

  // Test restoring a private window.
  sessionUpdatePromise = BrowserTestUtils.waitForSessionStoreUpdate(
    winData.win.gBrowser.selectedTab
  );
  await BrowserTestUtils.closeWindow(winData.win);
  await sessionUpdatePromise;

  is(
    SessionStore.getClosedWindowCount(),
    0,
    "The closed window was added to Recently Closed Windows"
  );

  // If the window gets restored, test will fail with an unclosed window.
  extension.sendMessage("restore");
  await extension.awaitMessage("done");

  await extension.unload();
});
