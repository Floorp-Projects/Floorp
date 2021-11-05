/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "DownloadSpamProtection",
  "resource:///modules/DownloadSpamProtection.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  PermissionTestUtils: "resource://testing-common/PermissionTestUtils.jsm",
});

const TEST_URI = "https://example.com";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  TEST_URI
);

add_task(async function setup() {
  PermissionTestUtils.add(
    TEST_URI,
    "automatic-download",
    Services.perms.UNKNOWN
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true], // To avoid the saving dialog being shown
      ["browser.download.enable_spam_prevention", true],
    ],
  });
});

add_task(async function check_download_spam_ui() {
  let spamProtection = DownloadIntegration.getDownloadSpamProtection();
  let oldFunction = spamProtection.observe.bind(spamProtection);
  let blockedDownloads = PromiseUtils.defer();
  let counter = 0;
  let newFunction = async (aSubject, aTopic, URL) => {
    await oldFunction(aSubject, aTopic, URL);
    counter++;
    if (counter == 99) {
      blockedDownloads.resolve();
    }
  };
  spamProtection.observe = newFunction;

  let newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "test_spammy_page.html"
  );

  registerCleanupFunction(async () => {
    spamProtection.observe = oldFunction;

    BrowserWindowTracker.getTopWindow().DownloadsPanel.hidePanel();
    DownloadIntegration.getDownloadSpamProtection().clearDownloadSpam(TEST_URI);
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    let downloads = await publicList.getAll();
    for (let download of downloads) {
      await publicList.remove(download);
      try {
        info("removing " + download.target.path);
        if (Services.appinfo.OS === "WINNT") {
          // We need to make the file writable to delete it on Windows.
          await IOUtils.setPermissions(download.target.path, 0o600);
        }
        await IOUtils.remove(download.target.path);
      } catch (ex) {
        info("The file " + download.target.path + " is not removed, " + ex);
      }
    }
    BrowserTestUtils.removeTab(newTab);
  });

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    {},
    newTab.linkedBrowser
  );

  await blockedDownloads.promise;

  let spamList = spamProtection.spamList;
  is(
    spamList._downloads[0].blockedDownloadsCount,
    99,
    "99 blocked downloads recorded"
  );
  ok(
    spamList._downloads[0].error.becauseBlockedByReputationCheck,
    "Download blocked because of reputation"
  );
  is(
    spamList._downloads[0].error.reputationCheckVerdict,
    "DownloadSpam",
    "Verdict is DownloadSpam"
  );

  let browserWin = BrowserWindowTracker.getTopWindow();
  await BrowserTestUtils.waitForPopupEvent(
    browserWin.DownloadsPanel.panel,
    "shown"
  );

  ok(browserWin.DownloadsPanel.isPanelShowing, "Download panel should open");

  let listbox = document.getElementById("downloadsListBox");
  ok(listbox, "Download list box present");

  await TestUtils.waitForCondition(() => {
    return listbox.childElementCount == 2;
  }, "2 downloads = 1 allowed download and 1 for 99 downloads blocked");

  let spamElement = listbox.itemChildren[0].classList.contains(
    "temporary-block"
  )
    ? listbox.itemChildren[0]
    : listbox.itemChildren[1];

  ok(spamElement.classList.contains("temporary-block"), "Download is blocked");
});
