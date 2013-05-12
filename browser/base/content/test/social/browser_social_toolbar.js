/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testProfileNone: function(next, useNull) {
    let profile = useNull ? null : {};
    Social.provider.updateUserProfile(profile);
    // check dom values
    let portrait = document.getElementsByClassName("social-statusarea-user-portrait")[0].getAttribute("src");
    // this is the default image for the profile area when not logged in.
    ok(!portrait, "portrait is empty");
    let userDetailsBroadcaster = document.getElementById("socialBroadcaster_userDetails");
    let notLoggedInStatusValue = userDetailsBroadcaster.getAttribute("notLoggedInLabel");
    let userButton = document.getElementsByClassName("social-statusarea-loggedInStatus")[0];
    ok(!userButton.hidden, "username is visible");
    is(userButton.getAttribute("label"), notLoggedInStatusValue, "label reflects not being logged in");
    next();
  },
  testProfileNull: function(next) {
    this.testProfileNone(next, true);
  },
  testProfileSet: function(next) {
    let profile = {
      portrait: "https://example.com/portrait.jpg",
      userName: "trickster",
      displayName: "Kuma Lisa",
      profileURL: "http://en.wikipedia.org/wiki/Kuma_Lisa"
    }
    Social.provider.updateUserProfile(profile);
    // check dom values
    let portrait = document.getElementsByClassName("social-statusarea-user-portrait")[0].getAttribute("src");
    is(profile.portrait, portrait, "portrait is set");
    let userButton = document.getElementsByClassName("social-statusarea-loggedInStatus")[0];
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
      is(socialToggleMore.querySelectorAll("menuitem").length, 6, "The minimum number of menuitems is two when there are no ambient notifications.");
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
      label: "Test Ambient 1 \u2046",
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
    let ambience4 = {
      name: "testIcon4",
      iconURL: "https://example.com/browser/browser/base/content/test/moz.png",
      contentPanel: "about:blank",
      counter: 0,
      label: "Test Ambient 4",
      menuURL: "https://example.com/testAmbient4"
    };
    Social.provider.setAmbientNotification(ambience);

    // for Bug 813834.  Check preference whether stored data is correct.
    is(JSON.parse(Services.prefs.getComplexValue("social.cached.ambientNotificationIcons", Ci.nsISupportsString).data).data.testIcon.label, "Test Ambient 1 \u2046", "label is stored into preference correctly");

    Social.provider.setAmbientNotification(ambience2);
    Social.provider.setAmbientNotification(ambience3);
    
    try {
      Social.provider.setAmbientNotification(ambience4);
    } catch(e) {}
    let numIcons = Object.keys(Social.provider.ambientNotificationIcons).length;
    ok(numIcons == 3, "prevent adding more than 3 ambient notification icons");

    let statusIcon = document.getElementById("social-provider-button").nextSibling;
    waitForCondition(function() {
      statusIcon = document.getElementById("social-provider-button").nextSibling;
      return !!statusIcon;
    }, function () {
      let badge = statusIcon.getAttribute("badge");
      is(badge, "42", "status value is correct");
      // If there is a counter, the aria-label should reflect it.
      is(statusIcon.getAttribute("aria-label"), "Test Ambient 1 \u2046 (42)");

      ambience.counter = 0;
      Social.provider.setAmbientNotification(ambience);
      badge = statusIcon.getAttribute("badge");
      is(badge, "", "status value is correct");
      // If there is no counter, the aria-label should be the same as the label
      is(statusIcon.getAttribute("aria-label"), "Test Ambient 1 \u2046");

      // The menu bar isn't as easy to instrument on Mac.
      if (navigator.platform.contains("Mac"))
        next();

      // Test that keyboard accessible menuitem was added.
      let toolsPopup = document.getElementById("menu_ToolsPopup");
      toolsPopup.addEventListener("popupshown", function ontoolspopupshownAmbient() {
        toolsPopup.removeEventListener("popupshown", ontoolspopupshownAmbient);
        let socialToggleMore = document.getElementById("menu_socialAmbientMenu");
        ok(socialToggleMore, "Keyboard accessible social menu should exist");
        is(socialToggleMore.querySelectorAll("menuitem").length, 9, "The number of menuitems is minimum plus three ambient notification menuitems.");
        is(socialToggleMore.hidden, false, "Menu is visible when ambient notifications have label & menuURL");
        let menuitem = socialToggleMore.querySelector(".ambient-menuitem");
        is(menuitem.getAttribute("label"), "Test Ambient 1 \u2046", "Keyboard accessible ambient menuitem should have specified label");
        toolsPopup.hidePopup();
        next();
      }, false);
      document.getElementById("menu_ToolsPopup").openPopup();
    }, "statusIcon was never found");
  },
  testProfileUnset: function(next) {
    Social.provider.updateUserProfile({});
    // check dom values
    let ambientIcons = document.querySelectorAll("#social-toolbar-item > box");
    for (let ambientIcon of ambientIcons) {
      ok(ambientIcon.collapsed, "ambient icon (" + ambientIcon.id + ") is collapsed");
    }
    
    next();
  },
  testMenuitemsExist: function(next) {
    let toggleSidebarMenuitems = document.getElementsByClassName("social-toggle-sidebar-menuitem");
    is(toggleSidebarMenuitems.length, 2, "Toggle Sidebar menuitems exist");
    let toggleDesktopNotificationsMenuitems = document.getElementsByClassName("social-toggle-notifications-menuitem");
    is(toggleDesktopNotificationsMenuitems.length, 2, "Toggle notifications menuitems exist");
    let toggleSocialMenuitems = document.getElementsByClassName("social-toggle-menuitem");
    is(toggleSocialMenuitems.length, 2, "Toggle Social menuitems exist");
    next();
  },
  testToggleNotifications: function(next) {
    let enabled = Services.prefs.getBoolPref("social.toast-notifications.enabled");
    let cmd = document.getElementById("Social:ToggleNotifications");
    is(cmd.getAttribute("checked"), enabled ? "true" : "false");
    enabled = !enabled;
    Services.prefs.setBoolPref("social.toast-notifications.enabled", enabled);
    is(cmd.getAttribute("checked"), enabled ? "true" : "false");
    Services.prefs.clearUserPref("social.toast-notifications.enabled");
    next();
  },
}
