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
    // The menu bar isn't as easy to instrument on Mac.
    if (navigator.platform.contains("Mac")) {
      info("Skipping checking the menubar on Mac OS");
      next();
    }

    // Test that keyboard accessible menuitem doesn't exist when no ambient icons specified.
    let toolsPopup = document.getElementById("menu_ToolsPopup");
    toolsPopup.addEventListener("popupshown", function ontoolspopupshownNoAmbient() {
      toolsPopup.removeEventListener("popupshown", ontoolspopupshownNoAmbient);
      let socialToggleMore = document.getElementById("menu_socialAmbientMenu");
      ok(socialToggleMore, "Keyboard accessible social menu should exist");
      is(socialToggleMore.querySelectorAll("menuitem").length, 2, "The minimum number of menuitems is two when there are no ambient notifications.");
      is(socialToggleMore.hidden, false, "Menu should be visible since we show some non-ambient notifications in the menu.");
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
    let ambience2 = {
      name: "testIcon2",
      iconURL: "https://example.com/browser/browser/base/content/test/moz.png",
      contentPanel: "about:blank",
      counter: 0,
      label: "Test Ambient 2",
      menuURL: "https://example.com/testAmbient2"
    };
    let ambience3 = {
      name: "testIcon3",
      iconURL: "https://example.com/browser/browser/base/content/test/moz.png",
      contentPanel: "about:blank",
      counter: 0,
      label: "Test Ambient 3",
      menuURL: "https://example.com/testAmbient3"
    };
    Social.provider.setAmbientNotification(ambience);
    Social.provider.setAmbientNotification(ambience2);
    Social.provider.setAmbientNotification(ambience3);

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

      // The menu bar isn't as easy to instrument on Mac.
      if (navigator.platform.contains("Mac"))
        next();

      // Test that keyboard accessible menuitem was added.
      let toolsPopup = document.getElementById("menu_ToolsPopup");
      toolsPopup.addEventListener("popupshown", function ontoolspopupshownAmbient() {
        toolsPopup.removeEventListener("popupshown", ontoolspopupshownAmbient);
        let socialToggleMore = document.getElementById("menu_socialAmbientMenu");
        ok(socialToggleMore, "Keyboard accessible social menu should exist");
        is(socialToggleMore.querySelectorAll("menuitem").length, 5, "The number of menuitems is minimum plus one ambient notification menuitem.");
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
    let toggleSidebarKeyboardMenuitem = document.getElementById("social-toggle-sidebar-keyboardmenuitem");
    ok(toggleSidebarKeyboardMenuitem, "Toggle Sidebar keyboard menuitem exists");
    next();
  },
  testShowDesktopNotificationsMenuitemExists: function(next) {
    let toggleDesktopNotificationsMenuitem = document.getElementById("social-toggle-notifications-menuitem");
    ok(toggleDesktopNotificationsMenuitem, "Toggle notifications menuitem exists");
    let toggleDesktopNotificationsKeyboardMenuitem = document.getElementById("social-toggle-notifications-keyboardmenuitem");
    ok(toggleDesktopNotificationsKeyboardMenuitem, "Toggle notifications keyboard menuitem exists");
    next();
  }
}

