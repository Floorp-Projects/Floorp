/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ControlCenter"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);
const { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

let { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);

const RESOURCE_PATH =
  "browser/browser/tools/mozscreenshots/mozscreenshots/extension/mozscreenshots/browser/resources/lib/controlCenter";
const HTTP_PAGE = "http://example.com/";
const HTTPS_PAGE = "https://example.com/";
const PERMISSIONS_PAGE = "https://test1.example.com/";
const HTTP_PASSWORD_PAGE = `http://test2.example.org/${RESOURCE_PATH}/password.html`;
const MIXED_CONTENT_URL = `https://example.com/${RESOURCE_PATH}/mixed.html`;
const MIXED_ACTIVE_CONTENT_URL = `https://example.com/${RESOURCE_PATH}/mixed_active.html`;
const MIXED_PASSIVE_CONTENT_URL = `https://example.com/${RESOURCE_PATH}/mixed_passive.html`;
const TRACKING_PAGE = `http://tracking.example.org/${RESOURCE_PATH}/tracking.html`;

var ControlCenter = {
  init(libDir) {},

  configurations: {
    about: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage("about:rights");
        await openIdentityPopup();
      },
    },

    localFile: {
      // This selector is different so we can exclude the changing file: path
      selectors: ["#identity-popup-security"],
      async applyConfig() {
        let channel = NetUtil.newChannel({
          uri: "resource://mozscreenshots/lib/mozscreenshots.html",
          loadUsingSystemPrincipal: true,
        });
        channel = channel.QueryInterface(Ci.nsIFileChannel);
        let browserWindow = Services.wm.getMostRecentWindow(
          "navigator:browser"
        );
        let gBrowser = browserWindow.gBrowser;
        BrowserTestUtils.loadURI(gBrowser.selectedBrowser, channel.file.path);
        await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        await openIdentityPopup();
      },
    },

    http: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(HTTP_PAGE);
        await openIdentityPopup();
      },
    },

    httpSubView: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(HTTP_PAGE);
        await openIdentityPopup(true);
      },
    },

    https: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(HTTPS_PAGE);
        await openIdentityPopup();
      },
    },

    httpsSubView: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(HTTPS_PAGE);
        await openIdentityPopup(true);
      },
    },

    singlePermission: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
          PERMISSIONS_PAGE
        );
        SitePermissions.setForPrincipal(
          principal,
          "camera",
          SitePermissions.ALLOW
        );

        await loadPage(PERMISSIONS_PAGE);
        await openIdentityPopup();
      },
    },

    allPermissions: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        // TODO: (Bug 1330601) Rewrite this to consider temporary (TAB) permission states.
        // There are 2 possible non-default permission states, so we alternate between them.
        let states = [SitePermissions.ALLOW, SitePermissions.BLOCK];
        let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
          PERMISSIONS_PAGE
        );
        SitePermissions.listPermissions().forEach(function(permission, index) {
          SitePermissions.setForPrincipal(
            principal,
            permission,
            states[index % 2]
          );
        });

        await loadPage(PERMISSIONS_PAGE);
        await openIdentityPopup();
      },
    },

    mixed: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_CONTENT_URL);
        await openIdentityPopup();
      },
    },

    mixedSubView: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_CONTENT_URL);
        await openIdentityPopup(true);
      },
    },

    mixedPassive: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_PASSIVE_CONTENT_URL);
        await openIdentityPopup();
      },
    },

    mixedPassiveSubView: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_PASSIVE_CONTENT_URL);
        await openIdentityPopup(true);
      },
    },

    mixedActive: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_ACTIVE_CONTENT_URL);
        await openIdentityPopup();
      },
    },

    mixedActiveSubView: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(MIXED_ACTIVE_CONTENT_URL);
        await openIdentityPopup(true);
      },
    },

    mixedActiveUnblocked: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow(
          "navigator:browser"
        );
        let gBrowser = browserWindow.gBrowser;
        await loadPage(MIXED_ACTIVE_CONTENT_URL);
        gBrowser.ownerGlobal.gIdentityHandler.disableMixedContentProtection();
        await BrowserTestUtils.browserLoaded(
          gBrowser.selectedBrowser,
          false,
          MIXED_ACTIVE_CONTENT_URL
        );
        await openIdentityPopup();
      },
    },

    mixedActiveUnblockedSubView: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow(
          "navigator:browser"
        );
        let gBrowser = browserWindow.gBrowser;
        await loadPage(MIXED_ACTIVE_CONTENT_URL);
        gBrowser.ownerGlobal.gIdentityHandler.disableMixedContentProtection();
        await BrowserTestUtils.browserLoaded(
          gBrowser.selectedBrowser,
          false,
          MIXED_ACTIVE_CONTENT_URL
        );
        await openIdentityPopup(true);
      },
    },

    httpPassword: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(HTTP_PASSWORD_PAGE);
        await openIdentityPopup();
      },
    },

    httpPasswordSubView: {
      selectors: ["#navigator-toolbox", "#identity-popup"],
      async applyConfig() {
        await loadPage(HTTP_PASSWORD_PAGE);
        await openIdentityPopup(true);
      },
    },

    trackingProtectionNoElements: {
      selectors: ["#navigator-toolbox", "#protections-popup"],
      async applyConfig() {
        Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);

        await loadPage(HTTP_PAGE);
        await openProtectionsPopup();
      },
    },

    trackingProtectionEnabled: {
      selectors: ["#navigator-toolbox", "#protections-popup"],
      async applyConfig() {
        Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);
        await UrlClassifierTestUtils.addTestTrackers();

        await loadPage(TRACKING_PAGE);
        await openProtectionsPopup();
      },
    },

    trackingProtectionDisabled: {
      selectors: ["#navigator-toolbox", "#protections-popup"],
      async applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow(
          "navigator:browser"
        );
        let gBrowser = browserWindow.gBrowser;
        Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);
        await UrlClassifierTestUtils.addTestTrackers();

        await loadPage(TRACKING_PAGE);

        // unblock the page
        let loaded = BrowserTestUtils.browserLoaded(
          gBrowser.selectedBrowser,
          false,
          TRACKING_PAGE
        );
        gBrowser.ownerGlobal.gProtectionsHandler.disableForCurrentPage();
        await loaded;
        await openProtectionsPopup();
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
    gIdentityHandler._identityPopup.classList.add("no-shadow");
  }
  gIdentityHandler._identityBox.querySelector("#identity-icon").click();
  if (expand) {
    // give some time for opening to avoid weird style issues
    await new Promise(c => setTimeout(c, 500));
    gIdentityHandler._identityPopup
      .querySelector("#identity-popup-security-expander")
      .click();
  }
}

async function openProtectionsPopup() {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let gBrowser = browserWindow.gBrowser;
  let { gProtectionsHandler } = gBrowser.ownerGlobal;
  gProtectionsHandler._protectionsPopup.hidePopup();
  // Disable the popup shadow on OSX until we have figured out bug 1425253.
  if (AppConstants.platform == "macosx") {
    gProtectionsHandler._protectionsPopup.classList.add("no-shadow");
  }
  gProtectionsHandler.showProtectionsPopup();
  // Wait for any animation.
  await new Promise(_ => setTimeout(_, 500));
}
