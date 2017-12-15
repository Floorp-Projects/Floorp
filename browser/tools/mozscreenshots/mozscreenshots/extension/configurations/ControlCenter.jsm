/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ControlCenter"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://testing-common/BrowserTestUtils.jsm");
Cu.import("resource:///modules/SitePermissions.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

let {UrlClassifierTestUtils} = Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

const RESOURCE_PATH = "browser/browser/tools/mozscreenshots/mozscreenshots/extension/mozscreenshots/browser/chrome/mozscreenshots/lib/controlCenter";
const HTTP_PAGE = "http://example.com/";
const HTTPS_PAGE = "https://example.com/";
const PERMISSIONS_PAGE = "https://test1.example.com/";
const HTTP_PASSWORD_PAGE = `http://test2.example.org/${RESOURCE_PATH}/password.html`;
const MIXED_CONTENT_URL = `https://example.com/${RESOURCE_PATH}/mixed.html`;
const MIXED_ACTIVE_CONTENT_URL = `https://example.com/${RESOURCE_PATH}/mixed_active.html`;
const MIXED_PASSIVE_CONTENT_URL = `https://example.com/${RESOURCE_PATH}/mixed_passive.html`;
const TRACKING_PAGE = `http://tracking.example.org/${RESOURCE_PATH}/tracking.html`;

this.ControlCenter = {
  init(libDir) { },

  configurations: {
    about: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage("about:rights");
        await openIdentityPopup();
      },
    },

    localFile: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        let channel = NetUtil.newChannel({
            uri: "chrome://mozscreenshots/content/lib/mozscreenshots.html",
            loadUsingSystemPrincipal: true
        });
        channel = channel.QueryInterface(Ci.nsIFileChannel);
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let gBrowser = browserWindow.gBrowser;
        BrowserTestUtils.loadURI(gBrowser.selectedBrowser, channel.file.path);
        await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        await openIdentityPopup();
      },

      async verifyConfig() {
        return Promise.reject("Bug 1373563: intermittent controlCenter_localFile on Taskcluster");
      },
    },

    http: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(HTTP_PAGE);
        await openIdentityPopup();
      },
    },

    httpSubView: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(HTTP_PAGE);
        await openIdentityPopup(true);
      },
    },

    https: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(HTTPS_PAGE);
        await openIdentityPopup();
      },
    },

    httpsSubView: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(HTTPS_PAGE);
        await openIdentityPopup(true);
      },
    },

    singlePermission: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        let uri = Services.io.newURI(PERMISSIONS_PAGE);
        SitePermissions.set(uri, "camera", SitePermissions.ALLOW);

        await loadPage(PERMISSIONS_PAGE);
        await openIdentityPopup();
      },
    },

    allPermissions: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        // TODO: (Bug 1330601) Rewrite this to consider temporary (TAB) permission states.
        // There are 2 possible non-default permission states, so we alternate between them.
        let states = [SitePermissions.ALLOW, SitePermissions.BLOCK];
        let uri = Services.io.newURI(PERMISSIONS_PAGE);
        SitePermissions.listPermissions().forEach(function(permission, index) {
          SitePermissions.set(uri, permission, states[index % 2]);
        });

        await loadPage(PERMISSIONS_PAGE);
        await openIdentityPopup();
      },
    },

    mixed: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_CONTENT_URL);
        await openIdentityPopup();
      },
    },

    mixedSubView: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_CONTENT_URL);
        await openIdentityPopup(true);
      },
    },

    mixedPassive: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_PASSIVE_CONTENT_URL);
        await openIdentityPopup();
      },
    },

    mixedPassiveSubView: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_PASSIVE_CONTENT_URL);
        await openIdentityPopup(true);
      },
    },

    mixedActive: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_ACTIVE_CONTENT_URL);
        await openIdentityPopup();
      },
    },

    mixedActiveSubView: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_ACTIVE_CONTENT_URL);
        await openIdentityPopup(true);
      },
    },

    mixedActiveUnblocked: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let gBrowser = browserWindow.gBrowser;
        await loadPage(MIXED_ACTIVE_CONTENT_URL);
        gBrowser.ownerGlobal.gIdentityHandler.disableMixedContentProtection();
        await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, MIXED_ACTIVE_CONTENT_URL);
        await openIdentityPopup();
      },
    },

    mixedActiveUnblockedSubView: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let gBrowser = browserWindow.gBrowser;
        await loadPage(MIXED_ACTIVE_CONTENT_URL);
        gBrowser.ownerGlobal.gIdentityHandler.disableMixedContentProtection();
        await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, MIXED_ACTIVE_CONTENT_URL);
        await openIdentityPopup(true);
      },
    },

    httpPassword: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(HTTP_PASSWORD_PAGE);
        await openIdentityPopup();
      },
    },

    httpPasswordSubView: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        await loadPage(HTTP_PASSWORD_PAGE);
        await openIdentityPopup(true);
      },
    },

    trackingProtectionNoElements: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);

        await loadPage(HTTP_PAGE);
        await openIdentityPopup();
      },
    },

    trackingProtectionEnabled: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);
        Services.prefs.setIntPref("privacy.trackingprotection.introCount", 20);
        await UrlClassifierTestUtils.addTestTrackers();

        await loadPage(TRACKING_PAGE);
        await openIdentityPopup();
      },
    },

    trackingProtectionDisabled: {
      selectors: ["#identity-popup"],
      async applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let gBrowser = browserWindow.gBrowser;
        Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);
        Services.prefs.setIntPref("privacy.trackingprotection.introCount", 20);
        await UrlClassifierTestUtils.addTestTrackers();

        await loadPage(TRACKING_PAGE);
        await openIdentityPopup();
        // unblock the page
        gBrowser.ownerGlobal.document.querySelector("#tracking-action-unblock").click();
        await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, TRACKING_PAGE);
        await openIdentityPopup();
      },
    },
  },
};

async function loadPage(url) {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let gBrowser = browserWindow.gBrowser;
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, url);
}

async function openIdentityPopup(expand) {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let gBrowser = browserWindow.gBrowser;
  let { gIdentityHandler } = gBrowser.ownerGlobal;
  gIdentityHandler._identityPopup.hidePopup();
  // Disable the popup shadow on OSX until we have figured out bug 1425253.
  if (AppConstants.platform == "macosx") {
    gIdentityHandler._identityPopup.style["-moz-window-shadow"] = "none";
  }
  gIdentityHandler._identityBox.querySelector("#identity-icon").click();
  if (expand) {
    // give some time for opening to avoid weird style issues
    await new Promise((c) => setTimeout(c, 500));
    gIdentityHandler._identityPopup.querySelector("#identity-popup-security-expander").click();
  }
}
