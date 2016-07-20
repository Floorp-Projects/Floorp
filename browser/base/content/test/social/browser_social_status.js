/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

var manifest = { // builtin provider
  name: "provider example.com",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
};
var manifest2 = { // used for testing install
  name: "provider test1",
  origin: "https://test1.example.com",
  statusURL: "https://test1.example.com/browser/browser/base/content/test/social/social_panel.html",
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


function openWindowAndWaitForInit(callback) {
  let topic = "browser-delayed-startup-finished";
  let w = OpenBrowserWindow();
  Services.obs.addObserver(function providerSet(subject, topic, data) {
    Services.obs.removeObserver(providerSet, topic);
    executeSoon(() => callback(w));
  }, topic, false);
}

function test() {
  waitForExplicitFinish();

  let frameScript = "data:,(" + function frame_script() {
    addMessageListener("socialTest-sendEvent", function(msg) {
      let data = msg.data;
      let evt = content.document.createEvent("CustomEvent");
      evt.initCustomEvent(data.name, true, true, JSON.stringify(data.data));
      content.document.documentElement.dispatchEvent(evt);
    });
  }.toString() + ")();";
  let mm = getGroupMessageManager("social");
  mm.loadFrameScript(frameScript, true);

  PopupNotifications.panel.setAttribute("animate", "false");
  registerCleanupFunction(function () {
    PopupNotifications.panel.removeAttribute("animate");
    mm.removeDelayedFrameScript(frameScript);
  });

  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, function () {
      Services.prefs.clearUserPref("social.remote-install.enabled");
      // just in case the tests failed, clear these here as well
      Services.prefs.clearUserPref("social.whitelist");
      CustomizableUI.reset();
      finishcb();
    });
  });
}

var tests = {
  testNoButtonOnEnable: function(next) {
    // we expect the addon install dialog to appear, we need to accept the
    // install from the dialog.
    let panel = document.getElementById("servicesInstall-notification");
    BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown").then(() => {
      info("servicesInstall-notification panel opened");
      panel.button.click();
    })

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
          let id = SocialStatus._toolbarHelper.idFromOrigin(provider.origin);
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
          let id = SocialStatus._toolbarHelper.idFromOrigin(manifest2.origin);
          let widget = CustomizableUI.getWidget(id).forWindow(window);
          ok(widget.node, "button added to widget set");
          checkSocialUI(window);
          BrowserTestUtils.removeTab(tab).then(next);
        });
      });
    });
  },
  testStatusPanel: function(next) {
    let icon = {
      name: "testIcon",
      iconURL: "chrome://browser/skin/Info.png",
      counter: 1
    };

    // click on panel to open and wait for visibility
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    let id = SocialStatus._toolbarHelper.idFromOrigin(manifest2.origin);
    let widget = CustomizableUI.getWidget(id);
    let btn = widget.forWindow(window).node;

    // Disable the transition
    let panel = document.getElementById("social-notification-panel");
    panel.setAttribute("animate", "false");
    BrowserTestUtils.waitForEvent(panel, "popupshown").then(() => {
      ensureFrameLoaded(panel.firstChild).then(() => {
        let mm = panel.firstChild.messageManager;
        mm.sendAsyncMessage("socialTest-sendEvent", { name: "Social:Notification", data: icon });
        BrowserTestUtils.waitForCondition(
          () => { return btn.getAttribute("badge"); }, "button updated by notification").then(() => {
            is(btn.style.listStyleImage, "url(\"" + icon.iconURL + "\")", "notification icon updated");
            panel.hidePopup();
          });
        });
    });
    BrowserTestUtils.waitForEvent(panel, "popuphidden").then(() => {
      panel.removeAttribute("animate");
      next();
    });
    btn.click(); // open the panel
  },

  testPanelOffline: function(next) {
    // click on panel to open and wait for visibility
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider.enabled, "provider is enabled");
    let id = SocialStatus._toolbarHelper.idFromOrigin(manifest2.origin);
    let widget = CustomizableUI.getWidget(id);
    let btn = widget.forWindow(window).node;
    ok(btn, "got a status button");
    let frameId = btn.getAttribute("notificationFrameId");
    let frame = document.getElementById(frameId);

    goOffline().then(function() {
      info("testing offline error page");
      // wait for popupshown
      let panel = document.getElementById("social-notification-panel");
      BrowserTestUtils.waitForEvent(panel, "popupshown").then(() => {
        ensureFrameLoaded(frame).then(() => {
          is(frame.contentDocument.documentURI.indexOf("about:socialerror?mode=tryAgainOnly"), 0, "social error page is showing "+frame.contentDocument.documentURI);
          // We got our error page, reset to avoid test leak.
          BrowserTestUtils.waitForEvent(frame, "load", true).then(() => {
            is(frame.contentDocument.documentURI, "about:blank", "closing error panel");
            BrowserTestUtils.waitForEvent(panel, "popuphidden").then(next);
            panel.hidePopup();
          });
          goOnline().then(() => {
            info("resetting error panel");
            frame.setAttribute("src", "about:blank");
          });
        });
      });
      // reload after going offline, wait for unload to open panel
      BrowserTestUtils.waitForEvent(frame, "unload", true).then(() => {
        btn.click();
      });
      frame.contentDocument.location.reload();
    });
  },

  testButtonOnDisable: function(next) {
    // enable the provider now
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider, "provider is installed");
    SocialService.disableProvider(manifest2.origin, function() {
      let id = SocialStatus._toolbarHelper.idFromOrigin(manifest2.origin);
      BrowserTestUtils.waitForCondition(() => { return !document.getElementById(id) },
                                        "button does not exist after disabling the provider").then(() => {
        Social.uninstallProvider(manifest2.origin, next);
       });
    });
  }
}
