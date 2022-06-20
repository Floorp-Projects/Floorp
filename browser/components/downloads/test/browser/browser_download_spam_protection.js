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

add_setup(async function() {
  // Create temp directory
  let time = new Date().getTime();
  let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempDir.append(time);
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, tempDir);

  PermissionTestUtils.add(
    TEST_URI,
    "automatic-download",
    Services.perms.UNKNOWN_ACTION
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.always_ask_before_handling_new_types", false], // To avoid the saving dialog being shown
      ["browser.download.enable_spam_prevention", true],
    ],
  });

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");
    await IOUtils.remove(tempDir.path, { recursive: true });
  });
});

add_task(async function check_download_spam_ui() {
  await task_resetState();

  registerCleanupFunction(async () => {
    BrowserWindowTracker.getTopWindow().DownloadsPanel.hidePanel();
    DownloadIntegration.downloadSpamProtection.clearDownloadSpam(TEST_URI);
    let publicList = await Downloads.getList(Downloads.PUBLIC);
    await publicList.removeFinished();
    BrowserTestUtils.removeTab(newTab);
  });
  let observedBlockedDownloads = 0;
  let gotAllBlockedDownloads = TestUtils.topicObserved(
    "blocked-automatic-download",
    () => {
      return ++observedBlockedDownloads >= 99;
    }
  );

  let newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "test_spammy_page.html"
  );

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    {},
    newTab.linkedBrowser
  );

  info("Waiting on all blocked downloads.");
  await gotAllBlockedDownloads;

  let spamProtection = DownloadIntegration.downloadSpamProtection;
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
    return listbox.childElementCount == 2 && !listbox.getAttribute("disabled");
  }, "2 downloads = 1 allowed download and 1 for 99 downloads blocked");

  let spamElement = listbox.itemChildren[0].classList.contains(
    "temporary-block"
  )
    ? listbox.itemChildren[0]
    : listbox.itemChildren[1];

  ok(spamElement.classList.contains("temporary-block"), "Download is blocked");
});
