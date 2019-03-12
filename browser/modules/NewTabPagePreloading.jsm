/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module is in charge of preloading 'new tab' pages for use when
 * the user opens a new tab.
 */

var EXPORTED_SYMBOLS = ["NewTabPagePreloading"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "E10SUtils", "resource://gre/modules/E10SUtils.jsm");

XPCOMUtils.defineLazyServiceGetters(this, {
  gAboutNewTabService: ["@mozilla.org/browser/aboutnewtab-service;1", "nsIAboutNewTabService"],
});

let NewTabPagePreloading = {
  get enabled() {
    return this.prefEnabled && this.newTabEnabled &&
      !gAboutNewTabService.overridden;
  },

  maybeCreatePreloadedBrowser(window) {
    if (!this.enabled || window.gBrowser.preloadedBrowser) {
      return;
    }

    const {gBrowser, gMultiProcessBrowser, BROWSER_NEW_TAB_URL} = window;
    let remoteType =
      E10SUtils.getRemoteTypeForURI(BROWSER_NEW_TAB_URL, gMultiProcessBrowser);
    let browser = gBrowser.createBrowser({ isPreloadBrowser: true, remoteType });
    gBrowser.preloadedBrowser = browser;

    let panel = gBrowser.getPanel(browser);
    gBrowser.tabpanels.appendChild(panel);

    if (remoteType != E10SUtils.NOT_REMOTE) {
      // For remote browsers, we need to make sure that the webProgress is
      // instantiated, otherwise the parent won't get informed about the state
      // of the preloaded browser until it gets attached to a tab.
      browser.webProgress;
    }

    browser.loadURI(BROWSER_NEW_TAB_URL, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    browser.docShellIsActive = false;
    browser._urlbarFocused = true;

    // Make sure the preloaded browser is loaded with desired zoom level
    let tabURI = Services.io.newURI(BROWSER_NEW_TAB_URL);
    window.FullZoom.onLocationChange(tabURI, false, browser);
  },

  getPreloadedBrowser(window) {
    if (!this.enabled) {
      return null;
    }

    // The preloaded browser might be null.
    let browser = window.gBrowser.preloadedBrowser;

    // Consume the browser.
    window.gBrowser.preloadedBrowser = null;

    // Attach the nsIFormFillController now that we know the browser
    // will be used. If we do that before and the preloaded browser
    // won't be consumed until shutdown then we leak a docShell.
    // Also, we do not need to take care of attaching nsIFormFillControllers
    // in the case that the browser is remote, as remote browsers take
    // care of that themselves.
    if (browser) {
      browser.setAttribute("preloadedState", "consumed");
      browser.setAttribute("autocompletepopup", "PopupAutoComplete");
    }

    return browser;
  },

  removePreloadedBrowser(window) {
    let browser = this.getPreloadedBrowser(window);
    if (browser) {
      window.gBrowser.getPanel(browser).remove();
    }
  },
};

XPCOMUtils.defineLazyPreferenceGetter(NewTabPagePreloading, "prefEnabled", "browser.newtab.preload", true);
XPCOMUtils.defineLazyPreferenceGetter(NewTabPagePreloading, "newTabEnabled", "browser.newtabpage.enabled", true);
