/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

var manifest2 = { // used for testing install
  name: "provider test1",
  origin: "https://test1.example.com",
  markURL: "https://test1.example.com/browser/browser/base/content/test/social/social_mark.html?url=%{url}",
  markedIcon: "https://test1.example.com/browser/browser/base/content/test/social/unchecked.jpg",
  unmarkedIcon: "https://test1.example.com/browser/browser/base/content/test/social/checked.jpg",

  iconURL: "https://test1.example.com/browser/browser/base/content/test/general/moz.png",
  version: "1.0"
};
var manifest3 = { // used for testing install
  name: "provider test2",
  origin: "https://test2.example.com",
  sidebarURL: "https://test2.example.com/browser/browser/base/content/test/social/social_sidebar.html",
  iconURL: "https://test2.example.com/browser/browser/base/content/test/general/moz.png",
  version: "1.0"
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

  PopupNotifications.panel.setAttribute("animate", "false");
  registerCleanupFunction(function () {
    PopupNotifications.panel.removeAttribute("animate");
    mm.removeDelayedFrameScript(frameScript);
  });

  runSocialTests(tests, undefined, undefined, finish);
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
    BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown").then(() => {
      info("servicesInstall-notification panel opened");
      panel.button.click();
    });

    let activationURL = manifest3.origin + "/browser/browser/base/content/test/social/social_activate.html"
    BrowserTestUtils.openNewForegroundTab(gBrowser, activationURL).then(tab => {
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
            BrowserTestUtils.removeTab(tab).then(next);
          });
        });
      });
    });
  },

  testButtonOnEnable: function(next) {
    let panel = document.getElementById("servicesInstall-notification");
    BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown").then(() => {
      info("servicesInstall-notification panel opened");
      panel.button.click();
    });

    // enable the provider now
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    BrowserTestUtils.openNewForegroundTab(gBrowser, activationURL).then(tab => {
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
          BrowserTestUtils.removeTab(tab).then(next);
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
    let ourTab;

    BrowserTestUtils.waitForEvent(btn.panel, "popupshown").then(() => {
      info("marks panel shown");
      let doc = btn.contentDocument;
      let unmarkBtn = doc.getElementById("unmark");
      ok(unmarkBtn, "testMarkPanel - got the panel unmark button");
      EventUtils.sendMouseEvent({type: "click"}, unmarkBtn, btn.contentWindow);
    });

    BrowserTestUtils.waitForEvent(btn.panel, "popuphidden").then(() => {
      BrowserTestUtils.removeTab(ourTab).then(() => {
        ok(btn.disabled, "button is disabled");
        next();
      });
    });

    // verify markbutton is disabled when there is no browser url
    ok(btn.disabled, "button is disabled");
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    BrowserTestUtils.openNewForegroundTab(gBrowser, activationURL).then(tab => {
      ourTab = tab;
      ok(!btn.disabled, "button is enabled");
      // first click marks the page, second click opens the page. We have to
      // synthesize so the command event happens
      EventUtils.synthesizeMouseAtCenter(btn, {});
      // wait for the button to be marked, click to open panel
      is(btn.panel.state, "closed", "panel should not be visible yet");
      BrowserTestUtils.waitForCondition(() => btn.isMarked, "button is marked").then(() => {
        EventUtils.synthesizeMouseAtCenter(btn, {});
      });
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
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html";
    BrowserTestUtils.openNewForegroundTab(gBrowser, activationURL).then(tab => {
      ok(!btn.disabled, "button is enabled");
      goOffline().then(function() {
        info("testing offline error page");
        // wait for popupshown
        BrowserTestUtils.waitForEvent(btn.panel, "popupshown").then(() => {
          info("marks panel is open");
          ensureFrameLoaded(btn.content).then(() => {
            is(btn.contentDocument.documentURI.indexOf("about:socialerror?mode=tryAgainOnly"), 0, "social error page is showing "+btn.contentDocument.documentURI);
            // cleanup after the page has been unmarked
            BrowserTestUtils.removeTab(tab).then(() => {
              ok(btn.disabled, "button is disabled");
              goOnline().then(next);
            });
          });
        });
        btn.markCurrentPage();
      });
    });
  },

  testButtonOnDisable: function(next) {
    // enable the provider now
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider, "provider is installed");
    SocialService.disableProvider(manifest2.origin, function() {
      let id = SocialMarks._toolbarHelper.idFromOrigin(manifest2.origin);
      BrowserTestUtils.waitForCondition(() => {
                        // getWidget now returns null since we've destroyed the widget
                        return !CustomizableUI.getWidget(id)
                       }, "button does not exist after disabling the provider").then(() => {
                         checkSocialUI(window);
                         Social.uninstallProvider(manifest2.origin, next);
                       });
    });
  }
}
