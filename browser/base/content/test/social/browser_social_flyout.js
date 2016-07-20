/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();
  let frameScript = "data:,(" + function frame_script() {
    addMessageListener("socialTest-CloseSelf", function(e) {
      content.close();
    });
    addMessageListener("socialTest-sendEvent", function(msg) {
      let data = msg.data;
      let evt = content.document.createEvent("CustomEvent");
      evt.initCustomEvent(data.name, true, true, JSON.stringify(data.data));
      content.document.documentElement.dispatchEvent(evt);
    });

  }.toString() + ")();";
  let mm = getGroupMessageManager("social");
  mm.loadFrameScript(frameScript, true);

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
    iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    SocialSidebar.show();
    ensureFrameLoaded(SocialSidebar.browser, manifest.sidebarURL).then(() => {
      // disable transitions for the test
      registerCleanupFunction(function () {
        SocialFlyout.panel.removeAttribute("animate");
      });
      SocialFlyout.panel.setAttribute("animate", "false");
      runSocialTests(tests, undefined, undefined, finishcb);
    });
  });
}

var tests = {
  testResizeFlyout: function(next) {
    let panel = document.getElementById("social-flyout-panel");

    BrowserTestUtils.waitForEvent(panel, "popupshown").then(() => {
      is(panel.firstChild.contentDocument.readyState, "complete", "panel is loaded prior to showing");
      // The width of the flyout should be 400px initially
      let iframe = panel.firstChild;
      let body = iframe.contentDocument.body;
      let cs = iframe.contentWindow.getComputedStyle(body);

      is(cs.width, "400px", "should be 400px wide");
      is(iframe.boxObject.width, 400, "iframe should now be 400px wide");
      is(cs.height, "400px", "should be 400px high");
      is(iframe.boxObject.height, 400, "iframe should now be 400px high");

      BrowserTestUtils.waitForEvent(iframe.contentWindow, "resize").then(() => {
        cs = iframe.contentWindow.getComputedStyle(body);

        is(cs.width, "500px", "should now be 500px wide");
        is(iframe.boxObject.width, 500, "iframe should now be 500px wide");
        is(cs.height, "500px", "should now be 500px high");
        is(iframe.boxObject.height, 500, "iframe should now be 500px high");
        BrowserTestUtils.waitForEvent(panel, "popuphidden").then(next);
        panel.hidePopup();
      });
      SocialFlyout.dispatchPanelEvent("socialTest-MakeWider");
    });

    SocialSidebar.browser.messageManager.sendAsyncMessage("socialTest-sendEvent", { name: "test-flyout-open", data: {} });
  },

  testCloseSelf: function(next) {
    let panel = document.getElementById("social-flyout-panel");
    BrowserTestUtils.waitForEvent(panel, "popupshown").then(() => {
      is(panel.firstChild.contentDocument.readyState, "complete", "panel is loaded prior to showing");
      BrowserTestUtils.waitForEvent(panel, "popuphidden").then(next);
      let mm = panel.firstChild.messageManager;
      mm.sendAsyncMessage("socialTest-CloseSelf", {});
    });
    SocialSidebar.browser.messageManager.sendAsyncMessage("socialTest-sendEvent", { name: "test-flyout-open", data: {} });
  },

  testCloseOnLinkTraversal: function(next) {

    BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen", true).then(event => {
      BrowserTestUtils.waitForCondition(function() { return panel.state == "closed" },
                                        "panel should close after tab open").then(() => {
        BrowserTestUtils.removeTab(event.target).then(next);
      });
    });

    let panel = document.getElementById("social-flyout-panel");
    BrowserTestUtils.waitForEvent(panel, "popupshown").then(() => {
      is(panel.firstChild.contentDocument.readyState, "complete", "panel is loaded prior to showing");
      is(panel.state, "open", "flyout should be open");
      let iframe = panel.firstChild;
      iframe.contentDocument.getElementById('traversal').click();
    });
    SocialSidebar.browser.messageManager.sendAsyncMessage("socialTest-sendEvent", { name: "test-flyout-open", data: {} });
  }
}
