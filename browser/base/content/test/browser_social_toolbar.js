/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    workerURL: "https://example.com/browser/browser/base/content/test/social_worker.js",
    iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testProfileSet: function(next) {
    let profile = {
      portrait: "https://example.com/portrait.jpg",
      userName: "trickster",
      displayName: "Kuma Lisa",
      profileURL: "http://en.wikipedia.org/wiki/Kuma_Lisa"
    }
    Social.provider.updateUserProfile(profile);
    // check dom values
    let portrait = document.getElementById("social-statusarea-user-portrait").getAttribute("src");
    is(profile.portrait, portrait, "portrait is set");
    let userButton = document.getElementById("social-statusarea-username");
    ok(!userButton.hidden, "username is visible");
    is(userButton.value, profile.userName, "username is set");
    next();
  },
  testNoAmbientNotificationsIsNoKeyboardMenu: function(next) {
    // Test that keyboard accessible menuitem doesn't exist when no ambient icons specified.
    let toolsPopup = document.getElementById("menu_ToolsPopup");
    toolsPopup.addEventListener("popupshown", function ontoolspopupshownNoAmbient() {
      toolsPopup.removeEventListener("popupshown", ontoolspopupshownNoAmbient);
      let socialToggleMore = document.getElementById("menu_socialAmbientMenu");
      ok(socialToggleMore, "Keyboard accessible social menu should exist");
      is(socialToggleMore.hidden, true, "Menu should be hidden when no ambient notifications.");
      toolsPopup.hidePopup();
      next();
    }, false);
    document.getElementById("menu_ToolsPopup").openPopup();
  },
  testAmbientNotifications: function(next) {
    let ambience = {
      name: "testIcon",
      iconURL: "https://example.com/browser/browser/base/content/test/moz.png",
      contentPanel: "about:blank",
      counter: 42,
      label: "Test Ambient 1",
      menuURL: "https://example.com/testAmbient1"
    };
    Social.provider.setAmbientNotification(ambience);

    let statusIcon = document.querySelector("#social-toolbar-item > box");
    waitForCondition(function() {
      statusIcon = document.querySelector("#social-toolbar-item > box");
      return !!statusIcon;
    }, function () {
      let statusIconLabel = statusIcon.querySelector("label");
      is(statusIconLabel.value, "42", "status value is correct");

      ambience.counter = 0;
      Social.provider.setAmbientNotification(ambience);
      is(statusIconLabel.value, "", "status value is correct");

      // Test that keyboard accessible menuitem was added.
      let toolsPopup = document.getElementById("menu_ToolsPopup");
      toolsPopup.addEventListener("popupshown", function ontoolspopupshownAmbient() {
        toolsPopup.removeEventListener("popupshown", ontoolspopupshownAmbient);
        let socialToggleMore = document.getElementById("menu_socialAmbientMenu");
        ok(socialToggleMore, "Keyboard accessible social menu should exist");
        is(socialToggleMore.hidden, false, "Menu is visible when ambient notifications have label & menuURL");
        let menuitem = socialToggleMore.querySelector("menuitem");
        is(menuitem.getAttribute("label"), "Test Ambient 1", "Keyboard accessible ambient menuitem should have specified label");
        toolsPopup.hidePopup();
        next();
      }, false);
      document.getElementById("menu_ToolsPopup").openPopup();
    }, "statusIcon was never found");
  },
  testProfileUnset: function(next) {
    Social.provider.updateUserProfile({});
    // check dom values
    let userButton = document.getElementById("social-statusarea-username");
    ok(userButton.hidden, "username is not visible");
    let ambientIcons = document.querySelectorAll("#social-toolbar-item > box");
    for (let ambientIcon of ambientIcons) {
      ok(ambientIcon.collapsed, "ambient icon (" + ambientIcon.id + ") is collapsed");
    }
    
    next();
  },
  testShowSidebarMenuitemExists: function(next) {
    let toggleSidebarMenuitem = document.getElementById("social-toggle-sidebar-menuitem");
    ok(toggleSidebarMenuitem, "Toggle Sidebar menuitem exists");
    next();
  },
  testShowDesktopNotificationsMenuitemExists: function(next) {
    let toggleDesktopNotificationsMenuitem = document.getElementById("social-toggle-notifications-menuitem");
    ok(toggleDesktopNotificationsMenuitem, "Toggle notifications menuitem exists");
    next();
  }
}

