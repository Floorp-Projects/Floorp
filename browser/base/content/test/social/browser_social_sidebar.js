/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

let manifest = { // normal provider
  name: "provider 1",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
};

function test() {
  waitForExplicitFinish();

  SocialService.addProvider(manifest, function() {
    // the test will remove the provider
    doTest();
  });
}

function doTest() {
  ok(SocialSidebar.canShow, "social sidebar should be able to be shown");
  ok(!SocialSidebar.opened, "social sidebar should not be open by default");
  SocialSidebar.show();

  let command = document.getElementById("Social:ToggleSidebar");
  let sidebar = document.getElementById("social-sidebar-box");
  let browser = sidebar.lastChild;

  function checkShown(shouldBeShown) {
    is(command.getAttribute("checked"), shouldBeShown ? "true" : "false",
       "toggle command should be " + (shouldBeShown ? "checked" : "unchecked"));
    is(sidebar.hidden, !shouldBeShown,
       "sidebar should be " + (shouldBeShown ? "visible" : "hidden"));
    if (shouldBeShown) {
      is(browser.getAttribute('src'), SocialSidebar.provider.sidebarURL, "sidebar url should be set");
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
        is(browser.getAttribute('src'), SocialSidebar.provider.sidebarURL, "sidebar url should be set");
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

      // disable social.
      SocialService.disableProvider(SocialSidebar.provider.origin, function() {
        checkShown(false);
        is(Social.providers.length, 0, "no providers left");
        defaultFinishChecks();
        // Finish the test
        executeSoon(finish);
      });
    });

    // Toggle it back on
    info("Toggling sidebar back on");
    SocialSidebar.toggleSidebar();
  });

  // use port messaging to ensure that the sidebar is both loaded and
  // ready before we run other tests
  let port = SocialSidebar.provider.getWorkerPort();
  port.postMessage({topic: "test-init"});
  port.onmessage = function (e) {
    let topic = e.data.topic;
    switch (topic) {
      case "got-sidebar-message":
        ok(true, "sidebar is loaded and ready");
        SocialSidebar.toggleSidebar();
    }
  };
}
