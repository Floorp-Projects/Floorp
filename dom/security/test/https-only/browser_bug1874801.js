/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Specifically test https://bugzilla.mozilla.org/show_bug.cgi?id=1874801

const TAB_URL =
  "https://example.com/browser/dom/security/test/https-only/file_bug1874801.html";

function assertImageLoaded(tab) {
  return ContentTask.spawn(tab.linkedBrowser, {}, () => {
    const img = content.document.getElementsByTagName("img")[0];

    ok(!!img, "Image tag should exist");
    ok(img.complete && img.naturalWidth > 0, "Image should have loaded ");
  });
}

add_task(async function test_bug1874801() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.mixed_content.upgrade_display_content", false],
      ["dom.security.https_first", true],
      ["dom.security.https_only_mode", true],
    ],
  });

  // Open Tab
  const tabToClose = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TAB_URL,
    true
  );

  // Make sure the image was loaded via HTTPS
  await assertImageLoaded(tabToClose);

  // Close Tab
  const tabClosePromise =
    BrowserTestUtils.waitForSessionStoreUpdate(tabToClose);
  BrowserTestUtils.removeTab(tabToClose);
  await tabClosePromise;

  // Restore Tab
  const restoredTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    TAB_URL,
    true
  );
  undoCloseTab();
  const restoredTab = await restoredTabPromise;

  // Make sure the image was loaded via HTTPS
  await assertImageLoaded(restoredTab);
});
