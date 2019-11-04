/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["LinkHandlerParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PlacesUIUtils",
  "resource:///modules/PlacesUIUtils.jsm"
);

let gTestListeners = new Set();

class LinkHandlerParent extends JSWindowActorParent {
  static addListenerForTests(listener) {
    gTestListeners.add(listener);
  }

  static removeListenerForTests(listener) {
    gTestListeners.delete(listener);
  }

  receiveMessage(aMsg) {
    let browser = this.browsingContext.top.embedderElement;
    if (!browser) {
      return;
    }

    if (browser.outerBrowser) {
      // Responsive design mode check
      browser = browser.outerBrowser;
    }

    let win = browser.ownerGlobal;

    let gBrowser = win.gBrowser;
    if (!gBrowser) {
      return;
    }

    switch (aMsg.name) {
      case "Link:LoadingIcon":
        if (aMsg.data.canUseForTab) {
          let tab = gBrowser.getTabForBrowser(browser);
          if (tab.hasAttribute("busy")) {
            tab.setAttribute("pendingicon", "true");
          }
        }

        this.notifyTestListeners("LoadingIcon", aMsg.data);
        break;

      case "Link:SetIcon":
        this.setIconFromLink(
          gBrowser,
          browser,
          aMsg.data.pageURL,
          aMsg.data.originalURL,
          aMsg.data.canUseForTab,
          aMsg.data.expiration,
          aMsg.data.iconURL
        );

        this.notifyTestListeners("SetIcon", aMsg.data);
        break;

      case "Link:SetFailedIcon":
        if (aMsg.data.canUseForTab) {
          this.clearPendingIcon(gBrowser, browser);
        }

        this.notifyTestListeners("SetFailedIcon", aMsg.data);
        break;

      case "Link:AddSearch":
        let tab = gBrowser.getTabForBrowser(browser);
        if (!tab) {
          break;
        }

        if (win.BrowserSearch) {
          win.BrowserSearch.addEngine(
            browser,
            aMsg.data.engine,
            Services.io.newURI(aMsg.data.url)
          );
        }
        break;
    }
  }

  notifyTestListeners(name, data) {
    for (let listener of gTestListeners) {
      listener(name, data);
    }
  }

  clearPendingIcon(gBrowser, aBrowser) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    tab.removeAttribute("pendingicon");
  }

  setIconFromLink(
    gBrowser,
    aBrowser,
    aPageURL,
    aOriginalURL,
    aCanUseForTab,
    aExpiration,
    aIconURL
  ) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (!tab) {
      return;
    }

    if (aCanUseForTab) {
      this.clearPendingIcon(gBrowser, aBrowser);
    }

    let iconURI;
    try {
      iconURI = Services.io.newURI(aIconURL);
    } catch (ex) {
      Cu.reportError(ex);
      return;
    }
    if (iconURI.scheme != "data") {
      try {
        Services.scriptSecurityManager.checkLoadURIWithPrincipal(
          aBrowser.contentPrincipal,
          iconURI,
          Services.scriptSecurityManager.ALLOW_CHROME
        );
      } catch (ex) {
        return;
      }
    }
    try {
      PlacesUIUtils.loadFavicon(
        aBrowser,
        Services.scriptSecurityManager.getSystemPrincipal(),
        Services.io.newURI(aPageURL),
        Services.io.newURI(aOriginalURL),
        aExpiration,
        iconURI
      );
    } catch (ex) {
      Cu.reportError(ex);
    }

    if (aCanUseForTab) {
      gBrowser.setIcon(tab, aIconURL, aOriginalURL);
    }
  }
}
