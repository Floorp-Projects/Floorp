/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  runSocialTestWithProvider(gProviders, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

let gProviders = [
  {
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html?provider1",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider 2",
    origin: "https://test1.example.com",
    sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html?provider2",
    workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  }
];

var tests = {
  testProviderSwitch: function(next) {
    function checkProviderMenu(selectedProvider) {
      let menu = document.getElementById("social-statusarea-popup");
      let menuProviders = menu.querySelectorAll(".social-provider-menuitem");
      is(menuProviders.length, gProviders.length, "correct number of providers listed in the menu");
      // Find the selectedProvider's menu item
      let el = menu.getElementsByAttribute("origin", selectedProvider.origin);
      is(el.length, 1, "selected provider menu item exists");
      is(el[0].getAttribute("checked"), "true", "selected provider menu item is checked");
    }

    checkProviderMenu(gProviders[0]);

    // Now wait for the initial provider profile to be set
    waitForProviderLoad(function() {
      checkUIStateMatchesProvider(gProviders[0]);

      // Now activate "provider 2"
      observeProviderSet(function () {
        waitForProviderLoad(function() {
          checkUIStateMatchesProvider(gProviders[1]);
          next();
        });
      });
      Social.activateFromOrigin("https://test1.example.com");
    });
  }
}

function checkUIStateMatchesProvider(provider) {
  let profileData = getExpectedProfileData(provider);
  // Bug 789863 - share button uses 'displayName', toolbar uses 'userName'
  // Check the "share button"
  let displayNameEl = document.getElementById("socialUserDisplayName");
  is(displayNameEl.getAttribute("label"), profileData.displayName, "display name matches provider profile");
  // The toolbar
  let loginStatus = document.getElementsByClassName("social-statusarea-loggedInStatus");
  for (let label of loginStatus) {
    is(label.value, profileData.userName, "username name matches provider profile");
  }
  // Sidebar
  is(document.getElementById("social-sidebar-browser").getAttribute("src"), provider.sidebarURL, "side bar URL is set");
}

function getExpectedProfileData(provider) {
  // This data is defined in social_worker.js
  if (provider.origin == "https://test1.example.com") {
    return {
      displayName: "Test1 User",
      userName: "tester"
    };
  }

  return {
    displayName: "Kuma Lisa",
    userName: "trickster"
  };
}

function observeProviderSet(cb) {
  Services.obs.addObserver(function providerSet(subject, topic, data) {
    Services.obs.removeObserver(providerSet, "social:provider-set");
    info("social:provider-set observer was notified");
    // executeSoon to let the browser UI observers run first
    executeSoon(cb);
  }, "social:provider-set", false);
}

function waitForProviderLoad(cb) {
  waitForCondition(function() {
    let sbrowser = document.getElementById("social-sidebar-browser");
    return Social.provider.profile &&
           Social.provider.profile.displayName &&
           sbrowser.docShellIsActive;
  }, cb, "waitForProviderLoad: provider profile was not set");
}
