/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let prefName = "social.enabled",
    gFinishCB;

function test() {
  waitForExplicitFinish();

  // Need to load a http/https/ftp/ftps page for the social share button to appear
  let tab = gBrowser.selectedTab = gBrowser.addTab("https://example.com", {skipAnimation: true});
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
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    gFinishCB = finishcb;
    testInitial();
  });
}

function testInitial(finishcb) {
  ok(Social.provider, "Social provider is active");
  ok(Social.provider.enabled, "Social provider is enabled");
  let port = Social.provider.getWorkerPort();
  ok(port, "Social provider has a port to its FrameWorker");
  port.close();

  let {shareButton, unsharePopup} = SocialShareButton;
  ok(shareButton, "share button exists");
  ok(unsharePopup, "share popup exists");

  let okButton = document.getElementById("unsharePopupContinueSharingButton");
  let undoButton = document.getElementById("unsharePopupStopSharingButton");
  let shareStatusLabel = document.getElementById("share-button-status");

  // ensure the worker initialization and handshakes are all done and we
  // have a profile and the worker has responsed to the recommend-prompt msg.
  waitForCondition(function() Social.provider.profile && Social.provider.recommendInfo != null, function() {
    is(shareButton.hasAttribute("shared"), false, "Share button should not have 'shared' attribute before share button is clicked");
    // check dom values
    let profile = Social.provider.profile;
    let portrait = document.getElementById("socialUserPortrait").getAttribute("src");
    is(profile.portrait, portrait, "portrait is set");
    let displayName = document.getElementById("socialUserDisplayName");
    is(displayName.label, profile.displayName, "display name is set");
    ok(!document.getElementById("unsharePopupHeader").hidden, "user profile is visible");

    // Check the strings from our worker actually ended up on the button.
    is(shareButton.getAttribute("tooltiptext"), "Share this page", "check tooltip text is correct");
    is(shareStatusLabel.getAttribute("value"), "", "check status label text is blank");
    // Check the relative URL was resolved correctly (note this image has offsets of zero...)
    is(shareButton.src, 'https://example.com/browser/browser/base/content/test/social/social_share_image.png', "check image url is correct");

    // Test clicking the share button
    shareButton.addEventListener("click", function listener() {
      shareButton.removeEventListener("click", listener);
      is(shareButton.hasAttribute("shared"), true, "Share button should have 'shared' attribute after share button is clicked");
      is(shareButton.getAttribute("tooltiptext"), "Unshare this page", "check tooltip text is correct");
      is(shareStatusLabel.getAttribute("value"), "This page has been shared", "check status label text is correct");
      // Check the URL and offsets were applied correctly
      is(shareButton.src, 'https://example.com/browser/browser/base/content/test/social/social_share_image.png', "check image url is correct");
      executeSoon(testSecondClick.bind(window, testPopupOKButton));
    });
    shareButton.click();
  }, "provider didn't provide user-recommend-prompt response");
}

function testSecondClick(nextTest) {
  let {shareButton, unsharePopup} = SocialShareButton;
  unsharePopup.addEventListener("popupshown", function listener() {
    unsharePopup.removeEventListener("popupshown", listener);
    ok(true, "popup was shown after second click");
    executeSoon(nextTest);
  });
  shareButton.click();
}

function testPopupOKButton() {
  let {shareButton, unsharePopup} = SocialShareButton;
  let okButton = document.getElementById("unsharePopupContinueSharingButton");
  unsharePopup.addEventListener("popuphidden", function listener() {
    unsharePopup.removeEventListener("popuphidden", listener);
    is(shareButton.hasAttribute("shared"), true, "Share button should still have 'shared' attribute after OK button is clicked");
    executeSoon(testSecondClick.bind(window, testPopupUndoButton));
  });
  okButton.click();
}

function testPopupUndoButton() {
  let {shareButton, unsharePopup} = SocialShareButton;
  let undoButton = document.getElementById("unsharePopupStopSharingButton");
  unsharePopup.addEventListener("popuphidden", function listener() {
    unsharePopup.removeEventListener("popuphidden", listener);
    is(shareButton.hasAttribute("shared"), false, "Share button should not have 'shared' attribute after Undo button is clicked");
    executeSoon(testShortcut);
  });
  undoButton.click();
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
  let {unsharePopup, shareButton} = SocialShareButton;
  is(shareButton.hasAttribute("shared"), true, "Share button should be in the 'shared' state after keyboard shortcut is used");

  // Test a second invocation of the shortcut
  unsharePopup.addEventListener("popupshown", function listener() {
    unsharePopup.removeEventListener("popupshown", listener);
    ok(true, "popup was shown after second use of keyboard shortcut");
    executeSoon(checkOKButton);
  });
  EventUtils.synthesizeKey("l", {accelKey: true, shiftKey: true}, keyTarget);
}

function checkOKButton() {
  let okButton = document.getElementById("unsharePopupContinueSharingButton");
  let undoButton = document.getElementById("unsharePopupStopSharingButton");
  is(document.activeElement, okButton, "ok button should be focused by default");

  // the undo button text, label text, access keys, etc  should be as
  // specified by the provider.
  function isEltAttr(eltid, attr, expected) {
    is(document.getElementById(eltid).getAttribute(attr), expected,
       "element '" + eltid + "' has correct value for attribute '" + attr + "'");
  }
  isEltAttr("socialUserRecommendedText", "value", "You have already shared this page");
  isEltAttr("unsharePopupContinueSharingButton", "label", "Got it!");
  isEltAttr("unsharePopupContinueSharingButton", "accesskey", "G");
  isEltAttr("unsharePopupStopSharingButton", "label", "Unshare it!");
  isEltAttr("unsharePopupStopSharingButton", "accesskey", "U");
  isEltAttr("socialUserPortrait", "aria-label", "Your pretty face");

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
  let unsharePopup = SocialShareButton.unsharePopup;
  is(document.activeElement.id, "unsharePopupContinueSharingButton", "testCloseBySpace, the ok button should be focused");
  unsharePopup.addEventListener("popuphidden", function listener() {
    unsharePopup.removeEventListener("popuphidden", listener);
    ok(true, "space closed the share popup");
    executeSoon(testStillSharedIn2Tabs);
  });
  EventUtils.synthesizeKey("VK_SPACE", {});
}

function testStillSharedIn2Tabs() {
  let toShare = "http://example.com";
  let {shareButton} = SocialShareButton;
  let initialTab = gBrowser.selectedTab;
  if (shareButton.hasAttribute("shared")) {
    SocialShareButton.unsharePage();
  }
  is(shareButton.hasAttribute("shared"), false, "Share button should not have 'shared' for the initial tab");
  let tab1 = gBrowser.selectedTab = gBrowser.addTab(toShare);
  let tab1b = gBrowser.getBrowserForTab(tab1);

  tab1b.addEventListener("load", function tabLoad(event) {
    tab1b.removeEventListener("load", tabLoad, true);
    let tab2 = gBrowser.selectedTab = gBrowser.addTab(toShare);
    let tab2b = gBrowser.getBrowserForTab(tab2);
    tab2b.addEventListener("load", function tabLoad(event) {
      tab2b.removeEventListener("load", tabLoad, true);
      // should start without either page being shared.
      is(shareButton.hasAttribute("shared"), false, "Share button should not have 'shared' before we've done anything");
      shareButton.click();
      is(shareButton.hasAttribute("shared"), true, "Share button should reflect the share");
      // and switching to the first tab (with the same URL) should still reflect shared.
      gBrowser.selectedTab = tab1;
      is(shareButton.hasAttribute("shared"), true, "Share button should reflect the share");
      // but switching back the initial one should reflect not shared.
      gBrowser.selectedTab = initialTab;
      is(shareButton.hasAttribute("shared"), false, "Initial tab should not reflect shared");

      gBrowser.selectedTab = tab1;
      SocialShareButton.unsharePage();
      gBrowser.removeTab(tab1);
      gBrowser.removeTab(tab2);
      executeSoon(testStillSharedAfterReopen);
    }, true);
  }, true);
}

function testStillSharedAfterReopen() {
  let toShare = "http://example.com";
  let {shareButton} = SocialShareButton;

  is(shareButton.hasAttribute("shared"), false, "Reopen: Share button should not have 'shared' for the initial tab");
  let tab = gBrowser.selectedTab = gBrowser.addTab(toShare);
  let tabb = gBrowser.getBrowserForTab(tab);
  tabb.addEventListener("load", function tabLoad(event) {
    tabb.removeEventListener("load", tabLoad, true);
    SocialShareButton.sharePage();
    is(shareButton.hasAttribute("shared"), true, "Share button should reflect the share");
    gBrowser.removeTab(tab);
    // should be on the initial unshared tab now.
    is(shareButton.hasAttribute("shared"), false, "Initial tab should be selected and be unshared.");
    // now open the same URL - should be back to shared.
    tab = gBrowser.selectedTab = gBrowser.addTab(toShare, {skipAnimation: true});
    tab.linkedBrowser.addEventListener("load", function tabLoad(event) {
      tab.linkedBrowser.removeEventListener("load", tabLoad, true);
      executeSoon(function() {
        is(shareButton.hasAttribute("shared"), true, "New tab to previously shared URL should reflect shared");
        SocialShareButton.unsharePage();
        gBrowser.removeTab(tab);
        executeSoon(testOnlyShareCertainUrlsTabSwitch);
      });
    }, true);
  }, true);
}

function testOnlyShareCertainUrlsTabSwitch() {
  let toShare = "http://example.com";
  let notSharable = "about:blank";
  let {shareButton} = SocialShareButton;
  let tab = gBrowser.selectedTab = gBrowser.addTab(toShare);
  let tabb = gBrowser.getBrowserForTab(tab);
  tabb.addEventListener("load", function tabLoad(event) {
    tabb.removeEventListener("load", tabLoad, true);
    ok(!shareButton.hidden, "share button not hidden for http url");
    let tab2 = gBrowser.selectedTab = gBrowser.addTab(notSharable);
    let tabb2 = gBrowser.getBrowserForTab(tab2);
    tabb2.addEventListener("load", function tabLoad(event) {
      tabb2.removeEventListener("load", tabLoad, true);
      ok(shareButton.hidden, "share button hidden for about:blank");
      gBrowser.selectedTab = tab;
      ok(!shareButton.hidden, "share button re-shown when switching back to http: url");
      gBrowser.selectedTab = tab2;
      ok(shareButton.hidden, "share button re-hidden when switching back to about:blank");
      gBrowser.removeTab(tab);
      gBrowser.removeTab(tab2);
      executeSoon(testOnlyShareCertainUrlsSameTab);
    }, true);
  }, true);
}

function testOnlyShareCertainUrlsSameTab() {
  let toShare = "http://example.com";
  let notSharable = "about:blank";
  let {shareButton} = SocialShareButton;
  let tab = gBrowser.selectedTab = gBrowser.addTab(toShare);
  let tabb = gBrowser.getBrowserForTab(tab);
  tabb.addEventListener("load", function tabLoad(event) {
    tabb.removeEventListener("load", tabLoad, true);
    ok(!shareButton.hidden, "share button not hidden for http url");
    tabb.addEventListener("load", function tabLoad(event) {
      tabb.removeEventListener("load", tabLoad, true);
      ok(shareButton.hidden, "share button hidden for about:blank");
      tabb.addEventListener("load", function tabLoad(event) {
        tabb.removeEventListener("load", tabLoad, true);
        ok(!shareButton.hidden, "share button re-enabled http url");
        gBrowser.removeTab(tab);
        executeSoon(testDisable);
      }, true);
      tabb.loadURI(toShare);
    }, true);
    tabb.loadURI(notSharable);
  }, true);
}

function testDisable() {
  let shareButton = SocialShareButton.shareButton;
  Services.prefs.setBoolPref(prefName, false);
  is(shareButton.hidden, true, "Share button should be hidden when pref is disabled");
  gFinishCB();
}
