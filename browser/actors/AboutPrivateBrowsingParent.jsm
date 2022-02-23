/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutPrivateBrowsingParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const SHOWN_PREF = "browser.search.separatePrivateDefault.ui.banner.shown";
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "MAX_SEARCH_BANNER_SHOW_COUNT",
  "browser.search.separatePrivateDefault.ui.banner.max",
  0
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "isPrivateSearchUIEnabled",
  "browser.search.separatePrivateDefault.ui.enabled",
  false
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Region: "resource://gre/modules/Region.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
});

// We only show the private search banner once per browser session.
let gSearchBannerShownThisSession;

class AboutPrivateBrowsingParent extends JSWindowActorParent {
  constructor() {
    super();
    Services.telemetry.setEventRecordingEnabled("aboutprivatebrowsing", true);
  }
  // Used by tests
  static setShownThisSession(shown) {
    gSearchBannerShownThisSession = shown;
  }

  receiveMessage(aMessage) {
    let browser = this.browsingContext.top.embedderElement;
    if (!browser) {
      return undefined;
    }

    let win = browser.ownerGlobal;

    switch (aMessage.name) {
      case "OpenPrivateWindow": {
        win.OpenBrowserWindow({ private: true });
        break;
      }
      case "OpenSearchPreferences": {
        win.openPreferences("search", { origin: "about-privatebrowsing" });
        break;
      }
      case "SearchHandoff": {
        let urlBar = win.gURLBar;
        let searchEngine = Services.search.defaultPrivateEngine;
        let isFirstChange = true;

        if (!aMessage.data || !aMessage.data.text) {
          urlBar.setHiddenFocus();
        } else {
          // Pass the provided text to the awesomebar
          urlBar.handoff(aMessage.data.text, searchEngine);
          isFirstChange = false;
        }

        let checkFirstChange = () => {
          // Check if this is the first change since we hidden focused. If it is,
          // remove hidden focus styles, prepend the search alias and hide the
          // in-content search.
          if (isFirstChange) {
            isFirstChange = false;
            urlBar.removeHiddenFocus(true);
            urlBar.handoff("", searchEngine);
            this.sendAsyncMessage("DisableSearch");
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

        let onDone = ev => {
          // We are done. Show in-content search again and cleanup.
          this.sendAsyncMessage("ShowSearch");

          const forceSuppressFocusBorder = ev?.type === "mousedown";
          urlBar.removeHiddenFocus(forceSuppressFocusBorder);

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
      case "ShouldShowSearch": {
        let engineName = Services.prefs.getStringPref(
          "browser.urlbar.placeholderName.private",
          ""
        );
        let shouldHandOffToSearchMode = UrlbarPrefs.get(
          "shouldHandOffToSearchMode"
        );
        return [engineName, shouldHandOffToSearchMode];
      }
      case "ShouldShowSearchBanner": {
        // If this is a pre-loaded private browsing new tab, then we don't want
        // to display the banner - it might never get displayed to the user
        // and we won't know, or it might get displayed at the wrong time.
        // This has the minor downside of not displaying the banner if
        // you go into private browsing via opening a link, and then opening
        // a new tab, we won't display the banner, for now, that's ok.
        if (browser.getAttribute("preloadedState") === "preloaded") {
          return null;
        }

        if (!isPrivateSearchUIEnabled || gSearchBannerShownThisSession) {
          return null;
        }
        gSearchBannerShownThisSession = true;
        const shownTimes = Services.prefs.getIntPref(SHOWN_PREF, 0);
        if (shownTimes >= MAX_SEARCH_BANNER_SHOW_COUNT) {
          return null;
        }
        Services.prefs.setIntPref(SHOWN_PREF, shownTimes + 1);
        return new Promise(resolve => {
          Services.search.getDefaultPrivate().then(engine => {
            resolve(engine.name);
          });
        });
      }
      case "SearchBannerDismissed": {
        Services.prefs.setIntPref(SHOWN_PREF, MAX_SEARCH_BANNER_SHOW_COUNT);
        break;
      }
      case "ShouldShowVPNPromo": {
        const homeRegion = Region.home || "";
        const currentRegion = Region.current || "";
        return (
          homeRegion.toLowerCase() !== "cn" &&
          currentRegion.toLowerCase() !== "cn" &&
          Services.policies.status !== Services.policies.ACTIVE
        );
      }
    }

    return undefined;
  }
}
