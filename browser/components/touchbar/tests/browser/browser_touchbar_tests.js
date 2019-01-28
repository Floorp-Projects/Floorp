/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_NAME = "ui.touchbar.layout";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "TouchBarHelper",
                                   "@mozilla.org/widget/touchbarhelper;1",
                                   "nsITouchBarHelper");
XPCOMUtils.defineLazyServiceGetter(this, "TouchBarInput",
                                   "@mozilla.org/widget/touchbarinput;1",
                                   "nsITouchBarInput");

function is_element_visible(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null when checking visibility");
  ok(!BrowserTestUtils.is_hidden(aElement), aMsg);
}

function is_element_hidden(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null when checking visibility");
  ok(BrowserTestUtils.is_hidden(aElement), aMsg);
}

/**
 * Sets the pref to contain errors. .layout should contain only the
 * non-erroneous elements.
 */
add_task(async function setWrongPref() {
  registerCleanupFunction(function() {
    Services.prefs.deleteBranch(PREF_NAME);
  });

  let wrongValue   = "Back, Back, Forwrd, NewTab, Unimplemented,";
  let correctValue = ["back", "new-tab"];
  let testValue = [];
  Services.prefs.setStringPref(PREF_NAME, wrongValue);

  let layoutItems = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  layoutItems = TouchBarHelper.layout;
  for (let i = 0; i < layoutItems.length; i++) {
    let input = layoutItems.queryElementAt(i, Ci.nsITouchBarInput);
    testValue.push(input.key);
  }
  Assert.equal(testValue.toString(),
               correctValue.toString(),
               "Pref should filter out incorrect inputs.");
});

/**
 * Tests if our bookmark button updates with our event.
 */
add_task(async function updateBookmarkButton() {
  Services.obs.notifyObservers(null, "bookmark-icon-updated", "starred");
  Assert.equal(TouchBarHelper.getTouchBarInput("AddBookmark").image,
    "bookmark-filled.pdf",
    "AddBookmark image should be filled bookmark after event.");

  Services.obs.notifyObservers(null, "bookmark-icon-updated", "unstarred");
  Assert.equal(TouchBarHelper.getTouchBarInput("AddBookmark").image,
    "bookmark.pdf",
    "AddBookmark image should be unfilled bookmark after event.");
});

/**
 * Tests if our Reader View button updates when a page can be reader viewed.
 */
add_task(async function updateReaderView() {
  const PREF_READERMODE = "reader.parse-on-load.enabled";
  await SpecialPowers.pushPrefEnv({set: [[PREF_READERMODE, true]]});

  // The page actions reader mode button
  var readerButton = document.getElementById("reader-mode-button");
  is_element_hidden(readerButton, "Reader Mode button should be hidden.");

  Assert.equal(TouchBarHelper.getTouchBarInput("ReaderView").disabled,
    true,
    "ReaderView Touch Bar button should be disabled by default.");

  const TEST_PATH = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content", "http://example.com");
  let url = TEST_PATH + "readerModeArticle.html";
  await BrowserTestUtils.withNewTab(url, async function() {
    await BrowserTestUtils.waitForCondition(() => !readerButton.hidden);

    Assert.equal(TouchBarHelper.getTouchBarInput("ReaderView").disabled, false,
      "ReaderView Touch Bar button should be enabled on reader-able pages.");
  });
});
