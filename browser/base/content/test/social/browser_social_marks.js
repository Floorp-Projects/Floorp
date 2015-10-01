/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

var manifest2 = { // used for testing install
  name: "provider test1",
  origin: "https://test1.example.com",
  workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
  markURL: "https://test1.example.com/browser/browser/base/content/test/social/social_mark.html?url=%{url}",
  markedIcon: "https://test1.example.com/browser/browser/base/content/test/social/unchecked.jpg",
  unmarkedIcon: "https://test1.example.com/browser/browser/base/content/test/social/checked.jpg",

  iconURL: "https://test1.example.com/browser/browser/base/content/test/general/moz.png",
  version: 1
};
var manifest3 = { // used for testing install
  name: "provider test2",
  origin: "https://test2.example.com",
  sidebarURL: "https://test2.example.com/browser/browser/base/content/test/social/social_sidebar.html",
  iconURL: "https://test2.example.com/browser/browser/base/content/test/general/moz.png",
  version: 1
};

function test() {
  waitForExplicitFinish();

  runSocialTests(tests, undefined, undefined, function () {
    ok(CustomizableUI.inDefaultState, "Should be in the default state when we finish");
    CustomizableUI.reset();
    finish();
  });
}

var tests = {
  testButtonDisabledOnActivate: function(next) {
    // starting on about:blank page, share should be visible but disabled when
    // adding provider
    is(gBrowser.selectedBrowser.currentURI.spec, "about:blank");
    SocialService.addProvider(manifest2, function(provider) {
      is(provider.origin, manifest2.origin, "provider is installed");
      let id = SocialMarks._toolbarHelper.idFromOrigin(manifest2.origin);
      let widget = CustomizableUI.getWidget(id).forWindow(window)
      ok(widget.node, "button added to widget set");

      // bypass widget go directly to dom, check attribute states
      let button = document.getElementById(id);
      is(button.disabled, true, "mark button is disabled");
      // verify the attribute for proper css
      is(button.getAttribute("disabled"), "true", "mark button attribute is disabled");
      // button should be visible
      is(button.hidden, false, "mark button is visible");

      checkSocialUI(window);
      SocialService.disableProvider(manifest2.origin, next);
    });
  },
  testNoButtonOnEnable: function(next) {
    // we expect the addon install dialog to appear, we need to accept the
    // install from the dialog.
    let panel = document.getElementById("servicesInstall-notification");
    ensureEventFired(PopupNotifications.panel, "popupshown").then(() => {
      info("servicesInstall-notification panel opened");
      panel.button.click();
    });

    let activationURL = manifest3.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      let data = {
        origin: doc.nodePrincipal.origin,
        url: doc.location.href,
        manifest: manifest3,
        window: window
      }

      Social.installProvider(data, function(addonManifest) {
        // enable the provider so we know the button would have appeared
        SocialService.enableProvider(manifest3.origin, function(provider) {
          is(provider.origin, manifest3.origin, "provider is installed");
          let id = SocialMarks._toolbarHelper.idFromOrigin(provider.origin);
          let widget = CustomizableUI.getWidget(id);
          ok(!widget || !widget.forWindow(window).node, "no button added to widget set");
          Social.uninstallProvider(manifest3.origin, function() {
            ensureBrowserTabClosed(tab).then(next);
          });
        });
      });
    });
  },

  testButtonOnEnable: function(next) {
    let panel = document.getElementById("servicesInstall-notification");
    ensureEventFired(PopupNotifications.panel, "popupshown").then(() => {
      info("servicesInstall-notification panel opened");
      panel.button.click();
    });

    // enable the provider now
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      let data = {
        origin: doc.nodePrincipal.origin,
        url: doc.location.href,
        manifest: manifest2,
        window: window
      }

      Social.installProvider(data, function(addonManifest) {
        SocialService.enableProvider(manifest2.origin, function(provider) {
          is(provider.origin, manifest2.origin, "provider is installed");
          let id = SocialMarks._toolbarHelper.idFromOrigin(manifest2.origin);
          let widget = CustomizableUI.getWidget(id).forWindow(window)
          ok(widget.node, "button added to widget set");

          // bypass widget go directly to dom, check attribute states
          let button = document.getElementById(id);
          is(button.disabled, false, "mark button is disabled");
          // verify the attribute for proper css
          ok(!button.hasAttribute("disabled"), "mark button attribute is disabled");
          // button should be visible
          is(button.hidden, false, "mark button is visible");

          checkSocialUI(window);
          ensureBrowserTabClosed(tab).then(next);
        });
      });
    });
  },

  testMarkPanel: function(next) {
    // click on panel to open and wait for visibility
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider.enabled, "provider is enabled");
    let id = SocialMarks._toolbarHelper.idFromOrigin(manifest2.origin);
    let widget = CustomizableUI.getWidget(id);
    let btn = widget.forWindow(window).node;
    ok(btn, "got a mark button");
    let port = provider.getWorkerPort();
    ok(port, "got a port");

    // verify markbutton is disabled when there is no browser url
    ok(btn.disabled, "button is disabled");
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      ok(!btn.disabled, "button is enabled");
      port.onmessage = function (e) {
        let topic = e.data.topic;
        switch (topic) {
          case "test-init-done":
            ok(true, "test-init-done received");
            ok(provider.profile.userName, "profile was set by test worker");
            // first click marks the page, second click opens the page. We have to
            // synthesize so the command event happens
            EventUtils.synthesizeMouseAtCenter(btn, {});
            // wait for the button to be marked, click to open panel
            is(btn.panel.state, "closed", "panel should not be visible yet");
            waitForCondition(() => btn.isMarked, function() {
              EventUtils.synthesizeMouseAtCenter(btn, {});
            }, "button is marked");
            break;
          case "got-social-panel-visibility":
            ok(true, "got the panel message " + e.data.result);
            if (e.data.result == "shown") {
              // unmark the page via the button in the page
              ensureFrameLoaded(btn.content).then(() => {
                let doc = btn.contentDocument;
                let unmarkBtn = doc.getElementById("unmark");
                ok(unmarkBtn, "testMarkPanel - got the panel unmark button");
                EventUtils.sendMouseEvent({type: "click"}, unmarkBtn, btn.contentWindow);
              });
            } else {
              // page should no longer be marked
              port.close();
              waitForCondition(() => !btn.isMarked, function() {
                // cleanup after the page has been unmarked
                ensureBrowserTabClosed(tab).then(() => {
                  ok(btn.disabled, "button is disabled");
                  next();
                });
              }, "button unmarked");
            }
            break;
        }
      };
      port.postMessage({topic: "test-init"});
    });
  },

  testMarkPanelOffline: function(next) {
    // click on panel to open and wait for visibility
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider.enabled, "provider is enabled");
    let id = SocialMarks._toolbarHelper.idFromOrigin(manifest2.origin);
    let widget = CustomizableUI.getWidget(id);
    let btn = widget.forWindow(window).node;
    ok(btn, "got a mark button");

    // verify markbutton is disabled when there is no browser url
    ok(btn.disabled, "button is disabled");
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      ok(!btn.disabled, "button is enabled");
      goOffline().then(function() {
        info("testing offline error page");
        // wait for popupshown
        ensureEventFired(btn.panel, "popupshown").then(() => {
          info("marks panel is open");
          ensureFrameLoaded(btn.content).then(() => {
            is(btn.contentDocument.documentURI.indexOf("about:socialerror?mode=tryAgainOnly"), 0, "social error page is showing "+btn.contentDocument.documentURI);
            // cleanup after the page has been unmarked
            ensureBrowserTabClosed(tab).then(() => {
              ok(btn.disabled, "button is disabled");
              goOnline().then(next);
            });
          });
        });
        btn.markCurrentPage();
      });
    });
  },

  testMarkPanelLoggedOut: function(next) {
    // click on panel to open and wait for visibility
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider.enabled, "provider is enabled");
    let id = SocialMarks._toolbarHelper.idFromOrigin(manifest2.origin);
    let widget = CustomizableUI.getWidget(id);
    let btn = widget.forWindow(window).node;
    ok(btn, "got a mark button");
    let port = provider.getWorkerPort();
    ok(port, "got a port");

    // verify markbutton is disabled when there is no browser url
    ok(btn.disabled, "button is disabled");
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      ok(!btn.disabled, "button is enabled");
      port.onmessage = function (e) {
        let topic = e.data.topic;
        switch (topic) {
          case "test-init-done":
            ok(true, "test-init-done received");
            ok(provider.profile.userName, "profile was set by test worker");
            port.postMessage({topic: "test-logout"});
            waitForCondition(() => !provider.profile.userName,
                function() {
                  // when the provider has not indicated to us that a user is
                  // logged in, the first click opens the page.
                  EventUtils.synthesizeMouseAtCenter(btn, {});
                },
                "profile was unset by test worker");
            break;
          case "got-social-panel-visibility":
            ok(true, "got the panel message " + e.data.result);
            if (e.data.result == "shown") {
              // our test marks the page during the load event (see
              // social_mark.html) regardless of login state, unmark the page
              // via the button in the page
              ensureFrameLoaded(btn.content).then(() => {
                let doc = btn.contentDocument;
                let unmarkBtn = doc.getElementById("unmark");
                ok(unmarkBtn, "testMarkPanelLoggedOut - got the panel unmark button");
                EventUtils.sendMouseEvent({type: "click"}, unmarkBtn, btn.contentWindow);
              });
            } else {
              // page should no longer be marked
              port.close();
              waitForCondition(() => !btn.isMarked, function() {
                // cleanup after the page has been unmarked
                ensureBrowserTabClosed(tab).then(() => {
                  ok(btn.disabled, "button is disabled");
                  next();
                });
              }, "button unmarked");
            }
            break;
        }
      };
      port.postMessage({topic: "test-init"});
    });
  },

  testButtonOnDisable: function(next) {
    // enable the provider now
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider, "provider is installed");
    SocialService.disableProvider(manifest2.origin, function() {
      let id = SocialMarks._toolbarHelper.idFromOrigin(manifest2.origin);
      waitForCondition(function() {
                        // getWidget now returns null since we've destroyed the widget
                        return !CustomizableUI.getWidget(id)
                       },
                       function() {
                         checkSocialUI(window);
                         Social.uninstallProvider(manifest2.origin, next);
                       }, "button does not exist after disabling the provider");
    });
  }
}
