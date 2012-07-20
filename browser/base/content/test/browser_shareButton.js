/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let prefName = "social.enabled",
    shareButton,
    sharePopup,
    okButton,
    undoButton;

function test() {
  waitForExplicitFinish();

  // Need to load a non-empty page for the social share button to appear
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:", {skipAnimation: true});
  tab.linkedBrowser.addEventListener("load", function tabLoad(event) {
    tab.linkedBrowser.removeEventListener("load", tabLoad, true);
    executeSoon(tabLoaded);
  }, true);

  // Enable the service to start
  Services.prefs.setBoolPref(prefName, true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(prefName);
    gBrowser.removeTab(tab);
  });
}

function tabLoaded() {
  ok(Social, "Social module loaded");

  // If the UI is already active, run the test immediately, otherwise wait
  // for initialization.
  if (Social.provider) {
    executeSoon(testInitial);
  } else {
    Services.obs.addObserver(function obs() {
      Services.obs.removeObserver(obs, "test-social-ui-ready");
      executeSoon(testInitial);
    }, "test-social-ui-ready", false);
  }
}

function testInitial() {
  ok(Social.provider, "Social provider is active");
  ok(Social.provider.enabled, "Social provider is enabled");
  ok(Social.provider.port, "Social provider has a port to its FrameWorker");

  shareButton = SocialShareButton.shareButton;
  sharePopup = SocialShareButton.sharePopup;
  ok(shareButton, "share button exists");
  ok(sharePopup, "share popup exists");

  okButton = document.getElementById("editSharePopupOkButton");
  undoButton = document.getElementById("editSharePopupUndoButton");

  is(shareButton.hidden, false, "share button should be visible");

  // Test clicking the share button
  shareButton.addEventListener("click", function listener() {
    shareButton.removeEventListener("click", listener);
    is(shareButton.hasAttribute("shared"), true, "Share button should have 'shared' attribute after share button is clicked");
    executeSoon(testSecondClick.bind(window, testPopupOKButton));
  });
  EventUtils.synthesizeMouseAtCenter(shareButton, {});
}

function testSecondClick(nextTest) {
  sharePopup.addEventListener("popupshown", function listener() {
    sharePopup.removeEventListener("popupshown", listener);
    ok(true, "popup was shown after second click");
    executeSoon(nextTest);
  });
  EventUtils.synthesizeMouseAtCenter(shareButton, {});
}

function testPopupOKButton() {
  sharePopup.addEventListener("popuphidden", function listener() {
    sharePopup.removeEventListener("popuphidden", listener);
    is(shareButton.hasAttribute("shared"), true, "Share button should still have 'shared' attribute after OK button is clicked");
    executeSoon(testSecondClick.bind(window, testPopupUndoButton));
  });
  EventUtils.synthesizeMouseAtCenter(okButton, {});
}

function testPopupUndoButton() {
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
  is(document.activeElement, okButton, "ok button should be focused by default");
  checkNextInTabOrder(undoButton, function () {
    checkNextInTabOrder(okButton, testCloseBySpace);
  });
}

function checkNextInTabOrder(element, next) {
  // This particular test doesn't really apply on Mac, since buttons aren't
  // focusable by default.
  if (navigator.platform.indexOf("Mac") != -1) {
    executeSoon(next);
    return;
  }

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
  is(document.activeElement.id, okButton.id, "testCloseBySpace, the ok button should be focused");
  sharePopup.addEventListener("popuphidden", function listener() {
    sharePopup.removeEventListener("popuphidden", listener);
    ok(true, "space closed the share popup");
    executeSoon(testDisable);
  });
  EventUtils.synthesizeKey("VK_SPACE", {});
}

function testDisable() {
  Services.prefs.setBoolPref(prefName, false);
  is(shareButton.hidden, true, "Share button should be hidden when pref is disabled");
  finish();
}
