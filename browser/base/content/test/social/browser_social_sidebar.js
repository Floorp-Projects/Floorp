/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

var manifest = { // normal provider
  name: "provider 1",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
};

function test() {
  waitForExplicitFinish();

  let frameScript = "data:,(" + function frame_script() {
    addEventListener("visibilitychange", function() {
      sendAsyncMessage("visibility", content.document.hidden ? "hidden" : "shown");
    });
  }.toString() + ")();";
  let mm = getGroupMessageManager("social");
  mm.loadFrameScript(frameScript, true);

  registerCleanupFunction(function () {
    mm.removeDelayedFrameScript(frameScript);
  });

  SocialService.addProvider(manifest, function() {
    // the test will remove the provider
    doTest();
  });
}

function doTest() {
  ok(SocialSidebar.canShow, "social sidebar should be able to be shown");
  ok(!SocialSidebar.opened, "social sidebar should not be open by default");

  let command = document.getElementById("Social:ToggleSidebar");
  let sidebar = document.getElementById("social-sidebar-box");
  let browser = sidebar.lastChild;
  ok(!browser.docShellIsActive, "sidebar is not active");
  is(sidebar.hidden, true, "sidebar should be hidden");
  is(command.getAttribute("checked"), "false", "toggle command should be unchecked");

  function checkShown(shouldBeShown) {
    is(command.getAttribute("checked"), shouldBeShown ? "true" : "false",
       "toggle command should be " + (shouldBeShown ? "checked" : "unchecked"));
    is(sidebar.hidden, !shouldBeShown,
       "sidebar should be " + (shouldBeShown ? "visible" : "hidden"));
    is(browser.docShellIsActive, shouldBeShown, "sidebar isActive in correct state");
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
  ensureFrameLoaded(browser).then(() => {
    // First check the the sidebar is initially visible, and loaded
    ok(!command.hidden, "toggle command should be visible");
    let mm = getGroupMessageManager("social");
    mm.addMessageListener("visibility", function shown(msg) {
      if (msg.data == "shown") {
        mm.removeMessageListener("visibility", shown);
        checkShown(true);
        info("Toggling sidebar to closed");
        SocialSidebar.toggleSidebar();
      }
    });
    mm.addMessageListener("visibility", function handler(msg) {
      if (msg.data == "hidden") {
        mm.removeMessageListener("visibility", handler);
        // disable social.
        SocialService.disableProvider(SocialSidebar.provider.origin, function() {
          checkShown(false);
          is(Social.providers.length, 0, "no providers left");
          defaultFinishChecks();
          // Finish the test
          executeSoon(finish);
        });
      }
    });
  });
  SocialSidebar.show();
}
