/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

const TEST_URL = getRootDirectory(gTestPath) + "browser_page_action_menu_share_win.html";

// Keep track of site details we are sharing
let sharedUrl, sharedTitle;

let stub = sinon.stub(BrowserPageActions.shareURL, "_windowsUIUtils").get(() => {
  return {
    shareUrl(url, title) {
      sharedUrl = url;
      sharedTitle = title;
    },
  };
});

registerCleanupFunction(async function() {
  stub.restore();
  delete window.sinon;
});

add_task(async function shareURL() {

  if (!AppConstants.isPlatformAndVersionAtLeast("win", "6.4")) {
    Assert.ok(true, "We only expose share on windows 10 and above");
    return;
  }

  await BrowserTestUtils.withNewTab(TEST_URL, async () => {
    // Open the panel.
    await promisePageActionPanelOpen();

    // Click Share URL.
    let shareURLButton = document.getElementById("pageAction-panel-shareURL");
    let hiddenPromise = promisePageActionPanelHidden();
    EventUtils.synthesizeMouseAtCenter(shareURLButton, {});

    await hiddenPromise;

    Assert.equal(sharedUrl, TEST_URL, "Shared correct URL");
    Assert.equal(sharedTitle, "Windows Sharing", "Shared with the correct title");
  });
});
