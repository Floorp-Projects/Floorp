/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
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
    // The sidebar.open pref only reflects the actual state of the sidebar
    // when social is enabled.
    if (Social.enabled)
      is(Services.prefs.getBoolPref("social.sidebar.open"), shouldBeShown,
         "sidebar open pref should be " + shouldBeShown);
    if (shouldBeShown) {
      is(browser.getAttribute('src'), Social.provider.sidebarURL, "sidebar url should be set");
      // We don't currently check docShellIsActive as this is only set
      // after load event fires, and the tests below explicitly wait for this
      // anyway.
    }
    else {
      ok(!browser.docShellIsActive, "sidebar should have an inactive docshell");
      // sidebar will only be immediately unloaded (and thus set to
      // about:blank) when canShow is false.
      if (SocialSidebar.canShow) {
        // should not have unloaded so will still be the provider URL.
        is(browser.getAttribute('src'), Social.provider.sidebarURL, "sidebar url should be set");
      } else {
        // should have been an immediate unload.
        is(browser.getAttribute('src'), "about:blank", "sidebar url should be blank");
      }
    }
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

      // Set Social.enabled = false and check everything is as expected.
      Social.enabled = false;
      checkShown(false);

      Social.enabled = true;
      checkShown(true);

      // And an edge-case - disable social and reset the provider.
      Social.provider = null;
      Social.enabled = false;
      checkShown(false);

      // Finish the test
      finishcb();
    });

    // Toggle it back on
    info("Toggling sidebar back on");
    Social.toggleSidebar();
  });

  // use port messaging to ensure that the sidebar is both loaded and
  // ready before we run other tests
  let port = Social.provider.getWorkerPort();
  port.postMessage({topic: "test-init"});
  port.onmessage = function (e) {
    let topic = e.data.topic;
    switch (topic) {
      case "got-sidebar-message":
        ok(true, "sidebar is loaded and ready");
        Social.toggleSidebar();
    }
  };
}

// XXX test sidebar in popup
