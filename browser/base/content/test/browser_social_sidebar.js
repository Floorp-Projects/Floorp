/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social_worker.js",
    iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
  };
  runSocialTestWithProvider(manifest, doTest);
}

function doTest(finishcb) {
  ok(SocialSidebar.canShow, "social sidebar should be able to be shown");
  ok(SocialSidebar.opened, "social sidebar should be open by default");

  let command = document.getElementById("Social:ToggleSidebar");
  let sidebar = document.getElementById("social-sidebar-box");
  let browser = sidebar.firstChild;

  function checkShown(shouldBeShown) {
    is(command.getAttribute("checked"), shouldBeShown ? "true" : "false",
       "toggle command should be " + (shouldBeShown ? "checked" : "unchecked"));
    is(sidebar.hidden, !shouldBeShown,
       "sidebar should be " + (shouldBeShown ? "visible" : "hidden"));
    is(Services.prefs.getBoolPref("social.sidebar.open"), shouldBeShown,
       "sidebar open pref should be " + shouldBeShown);
    if (shouldBeShown)
      is(browser.getAttribute('src'), Social.provider.sidebarURL, "sidebar url should be set");
  }

  // First check the the sidebar is initially visible, and loaded
  ok(!command.hidden, "toggle command should be visible");
  checkShown(true);

  browser.addEventListener("socialFrameHide", function sidebarhide() {
    browser.removeEventListener("socialFrameHide", sidebarhide);

    checkShown(false);

    browser.addEventListener("socialFrameShow", function sidebarshow() {
      browser.removeEventListener("socialFrameShow", sidebarshow);

      checkShown(true);

      // Finish the test
      finishcb();
    });

    // Toggle it back on
    info("Toggling sidebar back on");
    Social.toggleSidebar();
  });

  // Wait until the side bar loads
  waitForCondition(function () {
    return document.getElementById("social-sidebar-browser").docShellIsActive;
  }, function () {
    // Now toggle it off
    info("Toggling sidebar off");
    Social.toggleSidebar();
  });
}

// XXX test sidebar in popup
