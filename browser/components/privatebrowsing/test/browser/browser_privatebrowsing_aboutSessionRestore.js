/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks that the session restore button from about:sessionrestore
// is disabled in private mode
add_task(async function testNoSessionRestoreButton() {
  // Opening, then closing, a private window shouldn't create session data.
  (await BrowserTestUtils.openNewBrowserWindow({ private: true })).close();

  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let tab = BrowserTestUtils.addTab(win.gBrowser, "about:sessionrestore");
  let browser = tab.linkedBrowser;

  await BrowserTestUtils.browserLoaded(browser);

  await SpecialPowers.spawn(browser, [], async function () {
    Assert.ok(
      content.document.getElementById("errorTryAgain").disabled,
      "The Restore about:sessionrestore button should be disabled"
    );
  });

  win.close();
});
