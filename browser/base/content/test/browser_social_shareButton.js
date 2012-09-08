/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let prefName = "social.enabled",
    gFinishCB;

function test() {
  waitForExplicitFinish();

  // Need to load a non-empty page for the social share button to appear
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:", {skipAnimation: true});
  tab.linkedBrowser.addEventListener("load", function tabLoad(event) {
    tab.linkedBrowser.removeEventListener("load", tabLoad, true);
    executeSoon(tabLoaded);
  }, true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(prefName);
    gBrowser.removeTab(tab);
  });
}

function tabLoaded() {
  ok(Social, "Social module loaded");

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    gFinishCB = finishcb;
    testInitial();
  });
}

function testInitial(finishcb) {
  ok(Social.provider, "Social provider is active");
  ok(Social.provider.enabled, "Social provider is enabled");
  ok(Social.provider.port, "Social provider has a port to its FrameWorker");

  let {shareButton, sharePopup} = SocialShareButton;
  ok(shareButton, "share button exists");
  ok(sharePopup, "share popup exists");

  let okButton = document.getElementById("editSharePopupOkButton");
  let undoButton = document.getElementById("editSharePopupUndoButton");
  let shareStatusLabel = document.getElementById("share-button-status");

  // ensure the worker initialization and handshakes are all done and we
  // have a profile and the worker has responsed to the recommend-prompt msg.
  waitForCondition(function() Social.provider.profile && SocialShareButton.promptImages != null, function() {
    is(shareButton.hasAttribute("shared"), false, "Share button should not have 'shared' attribute before share button is clicked");
    // check dom values
    let profile = Social.provider.profile;
    let portrait = document.getElementById("socialUserPortrait").getAttribute("src");
    is(profile.portrait, portrait, "portrait is set");
    let displayName = document.getElementById("socialUserDisplayName");
    is(displayName.label, profile.displayName, "display name is set");
    ok(!document.getElementById("editSharePopupHeader").hidden, "user profile is visible");
  
    // Check the strings from our worker actually ended up on the button.
    is(shareButton.getAttribute("tooltiptext"), "Share this page", "check tooltip text is correct");
    is(shareStatusLabel.getAttribute("value"), "", "check status label text is blank");
    // Check the relative URL was resolved correctly (note this image has offsets of zero...)
    is(shareButton.style.backgroundImage, 'url("https://example.com/browser/browser/base/content/test/social_share_image.png")', "check image url is correct");

    // Test clicking the share button
    shareButton.addEventListener("click", function listener() {
      shareButton.removeEventListener("click", listener);
      is(shareButton.hasAttribute("shared"), true, "Share button should have 'shared' attribute after share button is clicked");
      is(shareButton.getAttribute("tooltiptext"), "Unshare this page", "check tooltip text is correct");
      is(shareStatusLabel.getAttribute("value"), "This page has been shared", "check status label text is correct");
      // Check the URL and offsets were applied correctly
      is(shareButton.style.backgroundImage, 'url("https://example.com/browser/browser/base/content/test/social_share_image.png")', "check image url is correct");
      executeSoon(testSecondClick.bind(window, testPopupOKButton));
    });
    EventUtils.synthesizeMouseAtCenter(shareButton, {});
  }, "provider didn't provide user-recommend-prompt response");
}

function testSecondClick(nextTest) {
  let {shareButton, sharePopup} = SocialShareButton;
  sharePopup.addEventListener("popupshown", function listener() {
    sharePopup.removeEventListener("popupshown", listener);
    ok(true, "popup was shown after second click");
    executeSoon(nextTest);
  });
  EventUtils.synthesizeMouseAtCenter(shareButton, {});
}

function testPopupOKButton() {
  let {shareButton, sharePopup} = SocialShareButton;
  let okButton = document.getElementById("editSharePopupOkButton");
  sharePopup.addEventListener("popuphidden", function listener() {
    sharePopup.removeEventListener("popuphidden", listener);
    is(shareButton.hasAttribute("shared"), true, "Share button should still have 'shared' attribute after OK button is clicked");
    executeSoon(testSecondClick.bind(window, testPopupUndoButton));
  });
  EventUtils.synthesizeMouseAtCenter(okButton, {});
}

function testPopupUndoButton() {
  let {shareButton, sharePopup} = SocialShareButton;
  let undoButton = document.getElementById("editSharePopupUndoButton");
  sharePopup.addEventListener("popuphidden", function listener() {
    sharePopup.removeEventListener("popuphidden", listener);
    is(shareButton.hasAttribute("shared"), false, "Share button should not have 'shared' attribute after Undo button is clicked");
    executeSoon(testShortcut);
  });
  EventUtils.synthesizeMouseAtCenter(undoButton, {});
}

function testShortcut() {
  let keyTarget = window;
  keyTarget.addEventListener("keyup", function listener() {
    keyTarget.removeEventListener("keyup", listener);
    executeSoon(checkShortcutWorked.bind(window, keyTarget));
  });
  EventUtils.synthesizeKey("l", {accelKey: true, shiftKey: true}, keyTarget);
}

function checkShortcutWorked(keyTarget) {
  let {sharePopup, shareButton} = SocialShareButton;
  is(shareButton.hasAttribute("shared"), true, "Share button should be in the 'shared' state after keyboard shortcut is used");

  // Test a second invocation of the shortcut
  sharePopup.addEventListener("popupshown", function listener() {
    sharePopup.removeEventListener("popupshown", listener);
    ok(true, "popup was shown after second use of keyboard shortcut");
    executeSoon(checkOKButton);
  });
  EventUtils.synthesizeKey("l", {accelKey: true, shiftKey: true}, keyTarget);
}

function checkOKButton() {
  let okButton = document.getElementById("editSharePopupOkButton");
  let undoButton = document.getElementById("editSharePopupUndoButton");
  is(document.activeElement, okButton, "ok button should be focused by default");

  // This rest of particular test doesn't really apply on Mac, since buttons
  // aren't focusable by default.
  if (navigator.platform.contains("Mac")) {
    executeSoon(testCloseBySpace);
    return;
  }

  let displayName = document.getElementById("socialUserDisplayName");

  // Linux has the buttons in the [unshare] [ok] order, so displayName will come first.
  if (navigator.platform.contains("Linux")) {
    checkNextInTabOrder(displayName, function () {
      checkNextInTabOrder(undoButton, function () {
        checkNextInTabOrder(okButton, testCloseBySpace);
      });
    });
  } else {
    checkNextInTabOrder(undoButton, function () {
      checkNextInTabOrder(displayName, function () {
        checkNextInTabOrder(okButton, testCloseBySpace);
      });
    });
  }
}

function checkNextInTabOrder(element, next) {
  function listener() {
    element.removeEventListener("focus", listener);
    is(document.activeElement, element, element.id + " should be next in tab order");
    executeSoon(next);
  }
  element.addEventListener("focus", listener);
  // Register a cleanup function to remove the listener in case this test fails
  registerCleanupFunction(function () {
    element.removeEventListener("focus", listener);
  });
  EventUtils.synthesizeKey("VK_TAB", {});
}

function testCloseBySpace() {
  let sharePopup = SocialShareButton.sharePopup;
  is(document.activeElement.id, "editSharePopupOkButton", "testCloseBySpace, the ok button should be focused");
  sharePopup.addEventListener("popuphidden", function listener() {
    sharePopup.removeEventListener("popuphidden", listener);
    ok(true, "space closed the share popup");
    executeSoon(testDisable);
  });
  EventUtils.synthesizeKey("VK_SPACE", {});
}

function testDisable() {
  let shareButton = SocialShareButton.shareButton;
  Services.prefs.setBoolPref(prefName, false);
  is(shareButton.hidden, true, "Share button should be hidden when pref is disabled");
  gFinishCB();
}
