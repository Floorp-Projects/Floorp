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
    iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testOpenCloseFlyout: function(next) {
    let panel = document.getElementById("social-flyout-panel");
    panel.addEventListener("popupshowing", function onShowing() {
      panel.removeEventListener("popupshowing", onShowing);
      is(panel.firstChild.contentDocument.readyState, "complete", "panel is loaded prior to showing");
    });
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-sidebar-message":
          port.postMessage({topic: "test-flyout-open"});
          break;
        case "got-flyout-visibility":
          if (e.data.result == "hidden") {
            ok(true, "flyout visibility is 'hidden'");
            is(panel.state, "closed", "panel really is closed");
            port.close();
            next();
          } else if (e.data.result == "shown") {
            ok(true, "flyout visibility is 'shown");
            port.postMessage({topic: "test-flyout-close"});
          }
          break;
        case "got-flyout-message":
          ok(e.data.result == "ok", "got flyout message");
          break;
      }
    }
    port.postMessage({topic: "test-init"});
  },

  testResizeFlyout: function(next) {
    let panel = document.getElementById("social-flyout-panel");
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          port.postMessage({topic: "test-flyout-open"});
          break;
        case "got-flyout-visibility":
          if (e.data.result != "shown")
            return;
          // The width of the flyout should be 400px initially
          let iframe = panel.firstChild;
          let body = iframe.contentDocument.body;
          let cs = iframe.contentWindow.getComputedStyle(body);

          is(cs.width, "400px", "should be 400px wide");
          is(iframe.boxObject.width, 400, "iframe should now be 400px wide");
          is(cs.height, "400px", "should be 400px high");
          is(iframe.boxObject.height, 400, "iframe should now be 400px high");

          iframe.contentWindow.addEventListener("resize", function _doneHandler() {
            iframe.contentWindow.removeEventListener("resize", _doneHandler, false);
            cs = iframe.contentWindow.getComputedStyle(body);

            is(cs.width, "500px", "should now be 500px wide");
            is(iframe.boxObject.width, 500, "iframe should now be 500px wide");
            is(cs.height, "500px", "should now be 500px high");
            is(iframe.boxObject.height, 500, "iframe should now be 500px high");
            panel.hidePopup();
            port.close();
            next();
          }, false);
          SocialFlyout.dispatchPanelEvent("socialTest-MakeWider");
          break;
      }
    }
    port.postMessage({topic: "test-init"});
  },

  testCloseSelf: function(next) {
    // window.close is affected by the pref dom.allow_scripts_to_close_windows,
    // which defaults to false, but is set to true by the test harness.
    // so temporarily set it back.
    const ALLOW_SCRIPTS_TO_CLOSE_PREF = "dom.allow_scripts_to_close_windows";
    // note clearUserPref doesn't do what we expect, as the test harness itself
    // changes the pref value - so clearUserPref resets it to false rather than
    // the true setup by the test harness.
    let oldAllowScriptsToClose = Services.prefs.getBoolPref(ALLOW_SCRIPTS_TO_CLOSE_PREF);    
    Services.prefs.setBoolPref(ALLOW_SCRIPTS_TO_CLOSE_PREF, false);
    let panel = document.getElementById("social-flyout-panel");
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          port.postMessage({topic: "test-flyout-open"});
          break;
        case "got-flyout-visibility":
          if (e.data.result != "shown")
            return;
          let iframe = panel.firstChild;
          iframe.contentDocument.addEventListener("SocialTest-DoneCloseSelf", function _doneHandler() {
            iframe.contentDocument.removeEventListener("SocialTest-DoneCloseSelf", _doneHandler, false);
            port.close();
            is(panel.state, "closed", "flyout should have closed itself");
            Services.prefs.setBoolPref(ALLOW_SCRIPTS_TO_CLOSE_PREF, oldAllowScriptsToClose);
            next();
          }, false);
          is(panel.state, "open", "flyout should be open");
          SocialFlyout.dispatchPanelEvent("socialTest-CloseSelf");
          break;
      }
    }
    port.postMessage({topic: "test-init"});
  },

  testCloseOnLinkTraversal: function(next) {

    function onTabOpen(event) {
      gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen, true);
      waitForCondition(function() { return panel.state == "closed" }, function() {
        gBrowser.removeTab(event.target);
        next();
      }, "panel should close after tab open");
    }

    let panel = document.getElementById("social-flyout-panel");
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          port.postMessage({topic: "test-flyout-open"});
          break;
        case "got-flyout-visibility":
          if (e.data.result == "shown") {
            // click on our test link
            is(panel.state, "open", "flyout should be open");
            gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen, true);
            let iframe = panel.firstChild;
            iframe.contentDocument.getElementById('traversal').click();
          }
          break;
      }
    }
    port.postMessage({topic: "test-init"});
  }
}
