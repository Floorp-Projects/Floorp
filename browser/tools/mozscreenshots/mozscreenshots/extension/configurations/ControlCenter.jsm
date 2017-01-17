/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ControlCenter"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://testing-common/BrowserTestUtils.jsm");
Cu.import("resource:///modules/SitePermissions.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

let {UrlClassifierTestUtils} = Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

const RESOURCE_PATH = "extensions/mozscreenshots/browser/chrome/mozscreenshots/lib/controlCenter";
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
      applyConfig: Task.async(function* () {
        yield loadPage("about:home");
        yield openIdentityPopup();
      }),
    },

    localFile: {
      applyConfig: Task.async(function* () {
        let channel = NetUtil.newChannel({
            uri: "chrome://mozscreenshots/content/lib/mozscreenshots.html",
            loadUsingSystemPrincipal: true
        });
        channel = channel.QueryInterface(Ci.nsIFileChannel);
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let gBrowser = browserWindow.gBrowser;
        BrowserTestUtils.loadURI(gBrowser.selectedBrowser, channel.file.path);
        yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        yield openIdentityPopup();
      }),
    },

    http: {
      applyConfig: Task.async(function* () {
        yield loadPage(HTTP_PAGE);
        yield openIdentityPopup();
      }),
    },

    httpSubView: {
      applyConfig: Task.async(function* () {
        yield loadPage(HTTP_PAGE);
        yield openIdentityPopup(true);
      }),
    },

    https: {
      applyConfig: Task.async(function* () {
        yield loadPage(HTTPS_PAGE);
        yield openIdentityPopup();
      }),
    },

    httpsSubView: {
      applyConfig: Task.async(function* () {
        yield loadPage(HTTPS_PAGE);
        yield openIdentityPopup(true);
      }),
    },

    singlePermission: {
      applyConfig: Task.async(function* () {
        let uri = Services.io.newURI(PERMISSIONS_PAGE)
        SitePermissions.set(uri, "camera", SitePermissions.ALLOW);

        yield loadPage(PERMISSIONS_PAGE);
        yield openIdentityPopup();
      }),
    },

    allPermissions: {
      applyConfig: Task.async(function* () {
        // TODO: (Bug 1330601) Rewrite this to consider temporary (TAB) permission states.
        // There are 2 possible non-default permission states, so we alternate between them.
        let states = [SitePermissions.ALLOW, SitePermissions.BLOCK];
        let uri = Services.io.newURI(PERMISSIONS_PAGE)
        SitePermissions.listPermissions().forEach(function(permission, index) {
          SitePermissions.set(uri, permission, states[index % 2]);
        });

        yield loadPage(PERMISSIONS_PAGE);
        yield openIdentityPopup();
      }),
    },

    mixed: {
      applyConfig: Task.async(function* () {
        yield loadPage(MIXED_CONTENT_URL);
        yield openIdentityPopup();
      }),
    },

    mixedSubView: {
      applyConfig: Task.async(function* () {
        yield loadPage(MIXED_CONTENT_URL);
        yield openIdentityPopup(true);
      }),
    },

    mixedPassive: {
      applyConfig: Task.async(function* () {
        yield loadPage(MIXED_PASSIVE_CONTENT_URL);
        yield openIdentityPopup();
      }),
    },

    mixedPassiveSubView: {
      applyConfig: Task.async(function* () {
        yield loadPage(MIXED_PASSIVE_CONTENT_URL);
        yield openIdentityPopup(true);
      }),
    },

    mixedActive: {
      applyConfig: Task.async(function* () {
        yield loadPage(MIXED_ACTIVE_CONTENT_URL);
        yield openIdentityPopup();
      }),
    },

    mixedActiveSubView: {
      applyConfig: Task.async(function* () {
        yield loadPage(MIXED_ACTIVE_CONTENT_URL);
        yield openIdentityPopup(true);
      }),
    },

    mixedActiveUnblocked: {
      applyConfig: Task.async(function* () {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let gBrowser = browserWindow.gBrowser;
        yield loadPage(MIXED_ACTIVE_CONTENT_URL);
        gBrowser.ownerGlobal.gIdentityHandler.disableMixedContentProtection();
        yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, MIXED_ACTIVE_CONTENT_URL);
        yield openIdentityPopup();
      }),
    },

    mixedActiveUnblockedSubView: {
      applyConfig: Task.async(function* () {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let gBrowser = browserWindow.gBrowser;
        yield loadPage(MIXED_ACTIVE_CONTENT_URL);
        gBrowser.ownerGlobal.gIdentityHandler.disableMixedContentProtection();
        yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, MIXED_ACTIVE_CONTENT_URL);
        yield openIdentityPopup(true);
      }),
    },

    httpPassword: {
      applyConfig: Task.async(function* () {
        yield loadPage(HTTP_PASSWORD_PAGE);
        yield openIdentityPopup();
      }),
    },

    httpPasswordSubView: {
      applyConfig: Task.async(function* () {
        yield loadPage(HTTP_PASSWORD_PAGE);
        yield openIdentityPopup(true);
      }),
    },

    trackingProtectionNoElements: {
      applyConfig: Task.async(function* () {
        Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);

        yield loadPage(HTTP_PAGE);
        yield openIdentityPopup();
      }),
    },

    trackingProtectionEnabled: {
      applyConfig: Task.async(function* () {
        Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);
        Services.prefs.setIntPref("privacy.trackingprotection.introCount", 20);
        yield UrlClassifierTestUtils.addTestTrackers();

        yield loadPage(TRACKING_PAGE);
        yield openIdentityPopup();
      }),
    },

    trackingProtectionDisabled: {
      applyConfig: Task.async(function* () {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let gBrowser = browserWindow.gBrowser;
        Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);
        Services.prefs.setIntPref("privacy.trackingprotection.introCount", 20);
        yield UrlClassifierTestUtils.addTestTrackers();

        yield loadPage(TRACKING_PAGE);
        yield openIdentityPopup();
        // unblock the page
        gBrowser.ownerGlobal.document.querySelector("#tracking-action-unblock").click();
        yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, TRACKING_PAGE);
        yield openIdentityPopup();
      }),
    },
  },
};

function* loadPage(url) {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let gBrowser = browserWindow.gBrowser;
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url);
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, url);
}

function* openIdentityPopup(expand) {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let gBrowser = browserWindow.gBrowser;
  let { gIdentityHandler } = gBrowser.ownerGlobal;
  gIdentityHandler._identityPopup.hidePopup();
  gIdentityHandler._identityBox.querySelector("#identity-icon").click();
  if (expand) {
    // give some time for opening to avoid weird style issues
    yield new Promise((c) => setTimeout(c, 500));
    gIdentityHandler._identityPopup.querySelector("#identity-popup-security-expander").click();
  }
}
