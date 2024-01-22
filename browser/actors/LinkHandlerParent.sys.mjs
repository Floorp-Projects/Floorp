/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
});

let gTestListeners = new Set();

export class LinkHandlerParent extends JSWindowActorParent {
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

    let win = browser.ownerGlobal;

    let gBrowser = win.gBrowser;

    switch (aMsg.name) {
      case "Link:LoadingIcon":
        if (!gBrowser) {
          return;
        }

        if (aMsg.data.canUseForTab) {
          let tab = gBrowser.getTabForBrowser(browser);
          if (tab.hasAttribute("busy")) {
            tab.setAttribute("pendingicon", "true");
          }
        }

        this.notifyTestListeners("LoadingIcon", aMsg.data);
        break;

      case "Link:SetIcon":
        // Cache the most recent icon and rich icon locally.
        if (aMsg.data.canUseForTab) {
          this.icon = aMsg.data;
        } else {
          this.richIcon = aMsg.data;
        }

        if (!gBrowser) {
          return;
        }

        this.setIconFromLink(gBrowser, browser, aMsg.data);

        this.notifyTestListeners("SetIcon", aMsg.data);
        break;

      case "Link:SetFailedIcon":
        if (!gBrowser) {
          return;
        }

        if (aMsg.data.canUseForTab) {
          this.clearPendingIcon(gBrowser, browser);
        }

        this.notifyTestListeners("SetFailedIcon", aMsg.data);
        break;

      case "Link:AddSearch":
        if (!gBrowser) {
          return;
        }

        let tab = gBrowser.getTabForBrowser(browser);
        if (!tab) {
          break;
        }

        if (win.BrowserSearch) {
          win.BrowserSearch.addEngine(browser, aMsg.data.engine);
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
    browser,
    {
      pageURL,
      originalURL,
      canUseForTab,
      expiration,
      iconURL,
      canStoreIcon,
      beforePageShow,
    }
  ) {
    let tab = gBrowser.getTabForBrowser(browser);
    if (!tab) {
      return;
    }

    if (canUseForTab) {
      this.clearPendingIcon(gBrowser, browser);
    }

    let iconURI;
    try {
      iconURI = Services.io.newURI(iconURL);
    } catch (ex) {
      console.error(ex);
      return;
    }
    if (iconURI.scheme != "data") {
      try {
        Services.scriptSecurityManager.checkLoadURIWithPrincipal(
          browser.contentPrincipal,
          iconURI,
          Services.scriptSecurityManager.ALLOW_CHROME
        );
      } catch (ex) {
        return;
      }
    }
    if (canStoreIcon) {
      try {
        lazy.PlacesUIUtils.loadFavicon(
          browser,
          Services.scriptSecurityManager.getSystemPrincipal(),
          Services.io.newURI(pageURL),
          Services.io.newURI(originalURL),
          expiration,
          iconURI
        );
      } catch (ex) {
        console.error(ex);
      }
    }

    if (canUseForTab) {
      gBrowser.setIcon(tab, iconURL, originalURL, null, beforePageShow);
    }
  }
}
