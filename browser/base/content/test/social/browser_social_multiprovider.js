/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();
  runSocialTestWithProvider(gProviders, function (finishcb) {
    SocialSidebar.provider = Social.providers[0];
    SocialSidebar.show();
    is(Social.providers[0].origin, SocialSidebar.provider.origin, "selected provider in sidebar");
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

let gProviders = [
  {
    name: "provider 1",
    origin: "https://test1.example.com",
    sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html?provider1",
    workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider 2",
    origin: "https://test2.example.com",
    sidebarURL: "https://test2.example.com/browser/browser/base/content/test/social/social_sidebar.html?provider2",
    workerURL: "https://test2.example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  }
];

var tests = {
  testProviderSwitch: function(next) {
    let menu = document.getElementById("social-statusarea-popup");
    let button = document.getElementById("social-sidebar-button");
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
      menu.removeEventListener("popupshown", theTest, true);
      menu.hidePopup(); // doesn't need visibility
      // first provider should already be visible in the sidebar
      is(Social.providers[0].origin, SocialSidebar.provider.origin, "selected provider in sidebar");
      checkProviderMenu(Social.providers[0]);

      // Now activate "provider 2"
      onSidebarLoad(function() {
        checkUIStateMatchesProvider(Social.providers[1]);

        onSidebarLoad(function() {
          checkUIStateMatchesProvider(Social.providers[0]);
          next();
        });

        // show the menu again so the menu is updated with the correct commands
        function doClick() {
          // click on the provider menuitem to switch providers
          let el = menu.getElementsByAttribute("origin", Social.providers[0].origin);
          is(el.length, 1, "selected provider menu item exists");
          EventUtils.synthesizeMouseAtCenter(el[0], {});
        }
        menu.addEventListener("popupshown", doClick, true);
        EventUtils.synthesizeMouseAtCenter(button, {});

      });
      SocialSidebar.provider = Social.providers[1];
    };
    menu.addEventListener("popupshown", theTest, true);
    EventUtils.synthesizeMouseAtCenter(button, {});
  }
}

function checkUIStateMatchesProvider(provider) {
  // Sidebar
  is(document.getElementById("social-sidebar-browser").getAttribute("src"), provider.sidebarURL, "side bar URL is set");
}

function onSidebarLoad(callback) {
  let sbrowser = document.getElementById("social-sidebar-browser");
  sbrowser.addEventListener("load", function load() {
    sbrowser.removeEventListener("load", load, true);
    // give the load a chance to finish before pulling the rug (ie. calling
    // next)
    executeSoon(callback);
  }, true);
}
