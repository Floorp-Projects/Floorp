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

var gProviders = [
  {
    name: "provider 1",
    origin: "https://test1.example.com",
    sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html?provider1",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider 2",
    origin: "https://test2.example.com",
    sidebarURL: "https://test2.example.com/browser/browser/base/content/test/social/social_sidebar.html?provider2",
    iconURL: "chrome://branding/content/icon48.png"
  }
];

var tests = {
  testProviderSwitch: function(next) {
    let sbrowser = document.getElementById("social-sidebar-browser");
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
    BrowserTestUtils.waitForEvent(menu, "popupshown", true).then(()=>{
      menu.hidePopup(); // doesn't need visibility
      // first provider should already be visible in the sidebar
      is(Social.providers[0].origin, SocialSidebar.provider.origin, "selected provider in sidebar");
      checkProviderMenu(Social.providers[0]);

      // Now activate "provider 2"
      BrowserTestUtils.waitForEvent(sbrowser, "load", true).then(()=>{
        checkUIStateMatchesProvider(Social.providers[1]);

        BrowserTestUtils.waitForEvent(sbrowser, "load", true).then(()=>{
          checkUIStateMatchesProvider(Social.providers[0]);
          next();
        });

        // show the menu again so the menu is updated with the correct commands
        BrowserTestUtils.waitForEvent(menu, "popupshown", true).then(()=>{
          // click on the provider menuitem to switch providers
          let el = menu.getElementsByAttribute("origin", Social.providers[0].origin);
          is(el.length, 1, "selected provider menu item exists");
          EventUtils.synthesizeMouseAtCenter(el[0], {});
        });
        EventUtils.synthesizeMouseAtCenter(button, {});
      });
      SocialSidebar.provider = Social.providers[1];
    });
    EventUtils.synthesizeMouseAtCenter(button, {});
  }
}

function checkUIStateMatchesProvider(provider) {
  // Sidebar
  is(document.getElementById("social-sidebar-browser").getAttribute("src"), provider.sidebarURL, "side bar URL is set");
}
