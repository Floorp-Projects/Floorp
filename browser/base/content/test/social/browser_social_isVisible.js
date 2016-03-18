/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
    iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
  };

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

  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testIsVisible: function(next) {
    let mm = getGroupMessageManager("social");
    mm.addMessageListener("visibility", function handler(msg) {
      mm.removeMessageListener("visibility", handler);
      is(msg.data, "shown", "sidebar is visible");
      next();
    });
    SocialSidebar.show();
  },
  testIsNotVisible: function(next) {
    let mm = getGroupMessageManager("social");
    mm.addMessageListener("visibility", function handler(msg) {
      mm.removeMessageListener("visibility", handler);
      is(msg.data, "hidden", "sidebar is hidden");
      next();
    });
    SocialSidebar.hide();
  }
}
