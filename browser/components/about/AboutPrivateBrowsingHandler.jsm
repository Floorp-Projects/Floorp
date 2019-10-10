/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutPrivateBrowsingHandler"];

const { RemotePages } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const SHOWN_PREF = "browser.search.separatePrivateDefault.ui.banner.shown";
const MAX_SEARCH_BANNER_SHOW_COUNT = 5;

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "isPrivateSearchUIEnabled",
  "browser.search.separatePrivateDefault.ui.enabled",
  false
);

var AboutPrivateBrowsingHandler = {
  _inited: false,
  // We only show the private search banner once per browser session.
  _searchBannerShownThisSession: false,
  _topics: [
    "OpenPrivateWindow",
    "OpenSearchPreferences",
    "SearchHandoff",
    "ShouldShowSearchBanner",
    "SearchBannerDismissed",
  ],

  init() {
    this.receiveMessage = this.receiveMessage.bind(this);
    this.pageListener = new RemotePages("about:privatebrowsing");
    for (let topic of this._topics) {
      this.pageListener.addMessageListener(topic, this.receiveMessage);
    }
    this._inited = true;
  },

  uninit() {
    if (!this._inited) {
      return;
    }
    for (let topic of this._topics) {
      this.pageListener.removeMessageListener(topic, this.receiveMessage);
    }
    this.pageListener.destroy();
  },

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "OpenPrivateWindow": {
        let win = aMessage.target.browser.ownerGlobal;
        win.OpenBrowserWindow({ private: true });
        break;
      }
      case "OpenSearchPreferences": {
        let win = aMessage.target.browser.ownerGlobal;
        win.openPreferences("search", { origin: "about-privatebrowsing" });
        break;
      }
      case "SearchHandoff": {
        let searchAlias = "";
        let searchAliases =
          Services.search.defaultPrivateEngine.wrappedJSObject
            .__internalAliases;
        if (searchAliases && searchAliases.length) {
          searchAlias = `${searchAliases[0]} `;
        }
        let urlBar = aMessage.target.browser.ownerGlobal.gURLBar;
        let isFirstChange = true;

        if (!aMessage.data || !aMessage.data.text) {
          urlBar.setHiddenFocus();
        } else {
          // Pass the provided text to the awesomebar. Prepend the @engine shortcut.
          urlBar.search(`${searchAlias}${aMessage.data.text}`);
          isFirstChange = false;
        }

        let checkFirstChange = () => {
          // Check if this is the first change since we hidden focused. If it is,
          // remove hidden focus styles, prepend the search alias and hide the
          // in-content search.
          if (isFirstChange) {
            isFirstChange = false;
            urlBar.removeHiddenFocus();
            urlBar.search(searchAlias);
            aMessage.target.sendAsyncMessage("HideSearch");
            urlBar.removeEventListener("compositionstart", checkFirstChange);
            urlBar.removeEventListener("paste", checkFirstChange);
          }
        };

        let onKeydown = ev => {
          // Check if the keydown will cause a value change.
          if (ev.key.length === 1 && !ev.altKey && !ev.ctrlKey && !ev.metaKey) {
            checkFirstChange();
          }
          // If the Esc button is pressed, we are done. Show in-content search and cleanup.
          if (ev.key === "Escape") {
            onDone();
          }
        };

        let onDone = () => {
          // We are done. Show in-content search again and cleanup.
          aMessage.target.sendAsyncMessage("ShowSearch");
          urlBar.removeHiddenFocus();

          urlBar.removeEventListener("keydown", onKeydown);
          urlBar.removeEventListener("mousedown", onDone);
          urlBar.removeEventListener("blur", onDone);
          urlBar.removeEventListener("compositionstart", checkFirstChange);
          urlBar.removeEventListener("paste", checkFirstChange);
        };

        urlBar.addEventListener("keydown", onKeydown);
        urlBar.addEventListener("mousedown", onDone);
        urlBar.addEventListener("blur", onDone);
        urlBar.addEventListener("compositionstart", checkFirstChange);
        urlBar.addEventListener("paste", checkFirstChange);
        break;
      }
      case "ShouldShowSearchBanner": {
        // If this is a pre-loaded private browsing new tab, then we don't want
        // to display the banner - it might never get displayed to the user
        // and we won't know, or it might get displayed at the wrong time.
        // This has the minor downside of not displaying the banner if
        // you go into private browsing via opening a link, and then opening
        // a new tab, we won't display the banner, for now, that's ok.
        if (
          aMessage.target.browser.getAttribute("preloadedState") === "preloaded"
        ) {
          aMessage.target.sendAsyncMessage("ShowSearchBanner", {
            show: false,
          });
          return;
        }

        if (!isPrivateSearchUIEnabled || this._searchBannerShownThisSession) {
          aMessage.target.sendAsyncMessage("ShowSearchBanner", {
            show: false,
          });
          return;
        }
        this._searchBannerShownThisSession = true;
        const shownTimes = Services.prefs.getIntPref(SHOWN_PREF, 0);
        if (shownTimes >= MAX_SEARCH_BANNER_SHOW_COUNT) {
          aMessage.target.sendAsyncMessage("ShowSearchBanner", {
            show: false,
          });
          return;
        }
        Services.prefs.setIntPref(SHOWN_PREF, shownTimes + 1);
        Services.search.getDefaultPrivate().then(engine => {
          aMessage.target.sendAsyncMessage("ShowSearchBanner", {
            engineName: engine.name,
            show: true,
          });
        });
        break;
      }
      case "SearchBannerDismissed": {
        Services.prefs.setIntPref(SHOWN_PREF, MAX_SEARCH_BANNER_SHOW_COUNT);
        break;
      }
    }
  },
};
