/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "TouchBarHelper",
  "@mozilla.org/widget/touchbarhelper;1",
  "nsITouchBarHelper"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "TouchBarInput",
  "@mozilla.org/widget/touchbarinput;1",
  "nsITouchBarInput"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

function is_element_visible(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null when checking visibility");
  ok(!BrowserTestUtils.is_hidden(aElement), aMsg);
}

function is_element_hidden(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null when checking visibility");
  ok(BrowserTestUtils.is_hidden(aElement), aMsg);
}

/**
 * Tests if our bookmark button updates with our event.
 */
add_task(async function updateBookmarkButton() {
  // We first check the default state. This also serves the purpose of forcing
  // nsITouchBarHelper to load on Macs without Touch Bars so that it will be
  // listening for "bookmark-icon-updated".
  Assert.equal(
    TouchBarHelper.getTouchBarInput("AddBookmark").image.spec,
    "chrome://browser/skin/bookmark-hollow.svg",
    "AddBookmark image should be unfilled bookmark after event."
  );

  Services.obs.notifyObservers(null, "bookmark-icon-updated", "starred");
  Assert.equal(
    TouchBarHelper.getTouchBarInput("AddBookmark").image.spec,
    "chrome://browser/skin/bookmark.svg",
    "AddBookmark image should be filled bookmark after event."
  );

  Services.obs.notifyObservers(null, "bookmark-icon-updated", "unstarred");
  Assert.equal(
    TouchBarHelper.getTouchBarInput("AddBookmark").image.spec,
    "chrome://browser/skin/bookmark-hollow.svg",
    "AddBookmark image should be unfilled bookmark after event."
  );
});

/**
 * Tests if our Reader View button updates when a page can be reader viewed.
 */
add_task(async function updateReaderView() {
  const PREF_READERMODE = "reader.parse-on-load.enabled";
  await SpecialPowers.pushPrefEnv({ set: [[PREF_READERMODE, true]] });

  // The page actions reader mode button
  var readerButton = document.getElementById("reader-mode-button");
  is_element_hidden(readerButton, "Reader Mode button should be hidden.");

  Assert.equal(
    TouchBarHelper.getTouchBarInput("ReaderView").disabled,
    true,
    "ReaderView Touch Bar button should be disabled by default."
  );

  let url = TEST_PATH + "readerModeArticle.html";
  await BrowserTestUtils.withNewTab(url, async function() {
    await BrowserTestUtils.waitForCondition(() => !readerButton.hidden);

    Assert.equal(
      TouchBarHelper.getTouchBarInput("ReaderView").disabled,
      false,
      "ReaderView Touch Bar button should be enabled on reader-able pages."
    );
  });
});

add_task(async function updateMainButtonInFullscreen() {
  Assert.equal(
    TouchBarHelper.getTouchBarInput("OpenLocation").image.spec,
    "chrome://global/skin/icons/search-glass.svg",
    "OpenLocation should be displaying the search glass icon."
  );
  BrowserTestUtils.loadURI(
    gBrowser.selectedBrowser,
    TEST_PATH + "video_test.html"
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  let entered = waitForFullScreenState(gBrowser.selectedBrowser, true);
  // Fullscreen video must be triggered from a user input handler so the video
  // page contains a script to enter fullscreen on Enter instead of us calling
  // requestFullscreen directly here.
  EventUtils.synthesizeKey("KEY_Enter");
  await entered;
  Assert.equal(
    TouchBarHelper.getTouchBarInput("OpenLocation").image.spec,
    "chrome://browser/skin/fullscreen-exit.svg",
    "OpenLocation should be displaying the exit fullscreen icon."
  );
  let exited = waitForFullScreenState(gBrowser.selectedBrowser, false);
  EventUtils.synthesizeKey("KEY_Enter");
  await exited;
  Assert.equal(
    TouchBarHelper.getTouchBarInput("OpenLocation").image.spec,
    "chrome://global/skin/icons/search-glass.svg",
    "OpenLocation should be displaying the search glass icon."
  );
});

function waitForFullScreenState(browser, state) {
  info("inside waitforfullscreenstate");
  return new Promise(resolve => {
    let eventReceived = false;

    let observe = (subject, topic, data) => {
      if (!eventReceived) {
        return;
      }
      Services.obs.removeObserver(observe, "fullscreen-painted");
      resolve();
    };
    Services.obs.addObserver(observe, "fullscreen-painted");

    window.addEventListener(
      `MozDOMFullscreen:${state ? "Entered" : "Exited"}`,
      () => {
        eventReceived = true;
      },
      { once: true }
    );
  });
}
