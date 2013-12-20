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
    let menu = document.getElementById("social-statusarea-popup");
    function checkProviderMenu(selectedProvider) {
      let menuProviders = menu.querySelectorAll(".social-provider-menuitem");
      is(menuProviders.length, gProviders.length, "correct number of providers listed in the menu");
      // Find the selectedProvider's menu item
      let el = menu.getElementsByAttribute("origin", selectedProvider.origin);
      is(el.length, 1, "selected provider menu item exists");
      is(el[0].getAttribute("checked"), "true", "selected provider menu item is checked");
    }

    // the menu is not populated until onpopupshowing, so wait for popupshown
    function theTest() {
      checkProviderMenu(gProviders[0]);

      // Now wait for the initial provider profile to be set
      waitForProviderLoad(function() {
        menu.removeEventListener("popupshown", theTest, true);
        checkUIStateMatchesProvider(gProviders[0]);

        // Now activate "provider 2"
        observeProviderSet(function () {
          waitForProviderLoad(function() {
            checkUIStateMatchesProvider(gProviders[1]);
            // disable social, click on the provider menuitem to switch providers
            Social.enabled = false;
            let el = menu.getElementsByAttribute("origin", gProviders[0].origin);
            is(el.length, 1, "selected provider menu item exists");
            el[0].click();
            waitForProviderLoad(function() {
              checkUIStateMatchesProvider(gProviders[0]);
              next();
            });
          });
        });
        Social.activateFromOrigin("https://test1.example.com");
      });
    };
    menu.addEventListener("popupshown", theTest, true);
    let button = document.getElementById("social-sidebar-button");
    EventUtils.synthesizeMouseAtCenter(button, {});
  }
}

function checkUIStateMatchesProvider(provider) {
  // Sidebar
  is(document.getElementById("social-sidebar-browser").getAttribute("src"), provider.sidebarURL, "side bar URL is set");
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
