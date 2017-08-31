/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks that the Session Restore "Restore Previous Session"
// button on about:home is disabled in private mode
add_task(async function test_no_sessionrestore_button() {
  // Activity Stream page does not have a restore session button.
  // We want to run this test only when Activity Stream is disabled from about:home.
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.newtabpage.activity-stream.aboutHome.enabled", false]
  ]});
  // Opening, then closing, a private window shouldn't create session data.
  (await BrowserTestUtils.openNewBrowserWindow({private: true})).close();

  let win = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let tab = win.gBrowser.addTab("about:home");
  let browser = tab.linkedBrowser;

  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, null, async function() {
    let button = content.document.getElementById("restorePreviousSession");
    Assert.equal(content.getComputedStyle(button).display, "none",
      "The Session Restore about:home button should be disabled");
  });

  win.close();
});
