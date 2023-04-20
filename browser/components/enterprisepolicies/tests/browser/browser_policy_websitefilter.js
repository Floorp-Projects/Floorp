/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const SUPPORT_FILES_PATH =
  "http://mochi.test:8888/browser/browser/components/enterprisepolicies/tests/browser/";
const BLOCKED_PAGE = "policy_websitefilter_block.html";
const EXCEPTION_PAGE = "policy_websitefilter_exception.html";
const SAVELINKAS_PAGE = "policy_websitefilter_savelink.html";

async function clearWebsiteFilter() {
  await setupPolicyEngineWithJson({
    policies: {
      WebsiteFilter: {
        Block: [],
        Exceptions: [],
      },
    },
  });
}

add_task(async function test_http() {
  await setupPolicyEngineWithJson({
    policies: {
      WebsiteFilter: {
        Block: ["*://mochi.test/*policy_websitefilter_*"],
        Exceptions: ["*://mochi.test/*_websitefilter_exception*"],
      },
    },
  });

  await checkBlockedPage(SUPPORT_FILES_PATH + BLOCKED_PAGE, true);
  await checkBlockedPage(
    "view-source:" + SUPPORT_FILES_PATH + BLOCKED_PAGE,
    true
  );
  await checkBlockedPage(
    "about:reader?url=" + SUPPORT_FILES_PATH + BLOCKED_PAGE,
    true
  );
  await checkBlockedPage(
    "about:READER?url=" + SUPPORT_FILES_PATH + BLOCKED_PAGE,
    true
  );
  await checkBlockedPage(SUPPORT_FILES_PATH + EXCEPTION_PAGE, false);

  await checkBlockedPage(SUPPORT_FILES_PATH + "301.sjs", true);

  await checkBlockedPage(SUPPORT_FILES_PATH + "302.sjs", true);
  await clearWebsiteFilter();
});

add_task(async function test_http_mixed_case() {
  await setupPolicyEngineWithJson({
    policies: {
      WebsiteFilter: {
        Block: ["*://mochi.test/*policy_websitefilter_*"],
        Exceptions: ["*://mochi.test/*_websitefilter_exception*"],
      },
    },
  });

  await checkBlockedPage(SUPPORT_FILES_PATH + BLOCKED_PAGE.toUpperCase(), true);
  await checkBlockedPage(
    SUPPORT_FILES_PATH + EXCEPTION_PAGE.toUpperCase(),
    false
  );
  await clearWebsiteFilter();
});

add_task(async function test_file() {
  await setupPolicyEngineWithJson({
    policies: {
      WebsiteFilter: {
        Block: ["file:///*"],
      },
    },
  });

  await checkBlockedPage("file:///this_should_be_blocked", true);
  await clearWebsiteFilter();
});

add_task(async function test_savelink() {
  await setupPolicyEngineWithJson({
    policies: {
      WebsiteFilter: {
        Block: ["*://mochi.test/*policy_websitefilter_block*"],
      },
    },
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    SUPPORT_FILES_PATH + SAVELINKAS_PAGE
  );

  let contextMenu = document.getElementById("contentAreaContextMenu");
  let promiseContextMenuOpen = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouse(
    "#savelink_blocked",
    0,
    0,
    {
      type: "contextmenu",
      button: 2,
      centered: true,
    },
    gBrowser.selectedBrowser
  );
  await promiseContextMenuOpen;

  let saveLink = document.getElementById("context-savelink");
  is(saveLink.disabled, true, "Save Link As should be disabled");

  let promiseContextMenuHidden = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await promiseContextMenuHidden;

  promiseContextMenuOpen = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouse(
    "#savelink_notblocked",
    0,
    0,
    {
      type: "contextmenu",
      button: 2,
      centered: true,
    },
    gBrowser.selectedBrowser
  );
  await promiseContextMenuOpen;

  saveLink = document.getElementById("context-savelink");
  is(saveLink.disabled, false, "Save Link As should not be disabled");

  promiseContextMenuHidden = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await promiseContextMenuHidden;

  BrowserTestUtils.removeTab(tab);
  await clearWebsiteFilter();
});

add_task(async function test_http_json_policy() {
  await setupPolicyEngineWithJson({
    policies: {
      WebsiteFilter: `{
        "Block": ["*://mochi.test/*policy_websitefilter_*"],
        "Exceptions": ["*://mochi.test/*_websitefilter_exception*"]
      }`,
    },
  });

  await checkBlockedPage(SUPPORT_FILES_PATH + BLOCKED_PAGE, true);
  await checkBlockedPage(
    "view-source:" + SUPPORT_FILES_PATH + BLOCKED_PAGE,
    true
  );
  await checkBlockedPage(
    "about:reader?url=" + SUPPORT_FILES_PATH + BLOCKED_PAGE,
    true
  );
  await checkBlockedPage(
    "about:READER?url=" + SUPPORT_FILES_PATH + BLOCKED_PAGE,
    true
  );
  await checkBlockedPage(SUPPORT_FILES_PATH + EXCEPTION_PAGE, false);

  await checkBlockedPage(SUPPORT_FILES_PATH + "301.sjs", true);

  await checkBlockedPage(SUPPORT_FILES_PATH + "302.sjs", true);
  await clearWebsiteFilter();
});
