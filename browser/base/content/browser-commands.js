/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

"use strict";

var kSkipCacheFlags =
  Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY |
  Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;

var BrowserCommands = {
  back(aEvent) {
    const where = BrowserUtils.whereToOpenLink(aEvent, false, true);

    if (where == "current") {
      try {
        gBrowser.goBack();
      } catch (ex) {}
    } else {
      duplicateTabIn(gBrowser.selectedTab, where, -1);
    }
  },

  forward(aEvent) {
    const where = BrowserUtils.whereToOpenLink(aEvent, false, true);

    if (where == "current") {
      try {
        gBrowser.goForward();
      } catch (ex) {}
    } else {
      duplicateTabIn(gBrowser.selectedTab, where, 1);
    }
  },

  handleBackspace() {
    switch (Services.prefs.getIntPref("browser.backspace_action")) {
      case 0:
        this.back();
        break;
      case 1:
        goDoCommand("cmd_scrollPageUp");
        break;
    }
  },

  handleShiftBackspace() {
    switch (Services.prefs.getIntPref("browser.backspace_action")) {
      case 0:
        this.forward();
        break;
      case 1:
        goDoCommand("cmd_scrollPageDown");
        break;
    }
  },

  gotoHistoryIndex(aEvent) {
    aEvent = BrowserUtils.getRootEvent(aEvent);

    const index = aEvent.target.getAttribute("index");
    if (!index) {
      return false;
    }

    const where = BrowserUtils.whereToOpenLink(aEvent);

    if (where == "current") {
      // Normal click. Go there in the current tab and update session history.

      try {
        gBrowser.gotoIndex(index);
      } catch (ex) {
        return false;
      }
      return true;
    }
    // Modified click. Go there in a new tab/window.

    const historyindex = aEvent.target.getAttribute("historyindex");
    duplicateTabIn(gBrowser.selectedTab, where, Number(historyindex));
    return true;
  },

  reloadOrDuplicate(aEvent) {
    aEvent = BrowserUtils.getRootEvent(aEvent);
    const accelKeyPressed =
      AppConstants.platform == "macosx" ? aEvent.metaKey : aEvent.ctrlKey;
    const backgroundTabModifier = aEvent.button == 1 || accelKeyPressed;

    if (aEvent.shiftKey && !backgroundTabModifier) {
      this.reloadSkipCache();
      return;
    }

    const where = BrowserUtils.whereToOpenLink(aEvent, false, true);
    if (where == "current") {
      this.reload();
    } else {
      duplicateTabIn(gBrowser.selectedTab, where);
    }
  },

  reload() {
    if (gBrowser.currentURI.schemeIs("view-source")) {
      // Bug 1167797: For view source, we always skip the cache
      this.reloadSkipCache();
      return;
    }
    this.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_NONE);
  },

  reloadSkipCache() {
    // Bypass proxy and cache.
    this.reloadWithFlags(kSkipCacheFlags);
  },

  reloadWithFlags(reloadFlags) {
    const unchangedRemoteness = [];

    for (const tab of gBrowser.selectedTabs) {
      const browser = tab.linkedBrowser;
      const url = browser.currentURI;
      const urlSpec = url.spec;
      // We need to cache the content principal here because the browser will be
      // reconstructed when the remoteness changes and the content prinicpal will
      // be cleared after reconstruction.
      const principal = tab.linkedBrowser.contentPrincipal;
      if (gBrowser.updateBrowserRemotenessByURL(browser, urlSpec)) {
        // If the remoteness has changed, the new browser doesn't have any
        // information of what was loaded before, so we need to load the previous
        // URL again.
        if (tab.linkedPanel) {
          loadBrowserURI(browser, url, principal);
        } else {
          // Shift to fully loaded browser and make
          // sure load handler is instantiated.
          tab.addEventListener(
            "SSTabRestoring",
            () => loadBrowserURI(browser, url, principal),
            { once: true }
          );
          gBrowser._insertBrowser(tab);
        }
      } else {
        unchangedRemoteness.push(tab);
      }
    }

    if (!unchangedRemoteness.length) {
      return;
    }

    // Reset temporary permissions on the remaining tabs to reload.
    // This is done here because we only want to reset
    // permissions on user reload.
    for (const tab of unchangedRemoteness) {
      SitePermissions.clearTemporaryBlockPermissions(tab.linkedBrowser);
      // Also reset DOS mitigations for the basic auth prompt on reload.
      delete tab.linkedBrowser.authPromptAbuseCounter;
    }
    gIdentityHandler.hidePopup();
    gPermissionPanel.hidePopup();

    const handlingUserInput = document.hasValidTransientUserGestureActivation;

    for (const tab of unchangedRemoteness) {
      if (tab.linkedPanel) {
        sendReloadMessage(tab);
      } else {
        // Shift to fully loaded browser and make
        // sure load handler is instantiated.
        tab.addEventListener("SSTabRestoring", () => sendReloadMessage(tab), {
          once: true,
        });
        gBrowser._insertBrowser(tab);
      }
    }

    function loadBrowserURI(browser, url, principal) {
      browser.loadURI(url, {
        flags: reloadFlags,
        triggeringPrincipal: principal,
      });
    }

    function sendReloadMessage(tab) {
      tab.linkedBrowser.sendMessageToActor(
        "Browser:Reload",
        { flags: reloadFlags, handlingUserInput },
        "BrowserTab"
      );
    }
  },

  home(aEvent) {
    if (aEvent?.button == 2) {
      // right-click: do nothing
      return;
    }

    const homePage = HomePage.get(window);
    let where = BrowserUtils.whereToOpenLink(aEvent, false, true);

    // Don't load the home page in pinned or hidden tabs (e.g. Firefox View).
    if (
      where == "current" &&
      (gBrowser?.selectedTab.pinned || gBrowser?.selectedTab.hidden)
    ) {
      where = "tab";
    }

    // openTrustedLinkIn in utilityOverlay.js doesn't handle loading multiple pages
    let notifyObservers;
    switch (where) {
      case "current":
        // If we're going to load an initial page in the current tab as the
        // home page, we set initialPageLoadedFromURLBar so that the URL
        // bar is cleared properly (even during a remoteness flip).
        if (isInitialPage(homePage)) {
          gBrowser.selectedBrowser.initialPageLoadedFromUserAction = homePage;
        }
        loadOneOrMoreURIs(
          homePage,
          Services.scriptSecurityManager.getSystemPrincipal(),
          null
        );
        if (isBlankPageURL(homePage)) {
          gURLBar.select();
        } else {
          gBrowser.selectedBrowser.focus();
        }
        notifyObservers = true;
        aEvent?.preventDefault();
        break;
      case "tabshifted":
      case "tab": {
        const urls = homePage.split("|");
        const loadInBackground = Services.prefs.getBoolPref(
          "browser.tabs.loadBookmarksInBackground",
          false
        );
        // The homepage observer event should only be triggered when the homepage opens
        // in the foreground. This is mostly to support the homepage changed by extension
        // doorhanger which doesn't currently support background pages. This may change in
        // bug 1438396.
        notifyObservers = !loadInBackground;
        gBrowser.loadTabs(urls, {
          inBackground: loadInBackground,
          triggeringPrincipal:
            Services.scriptSecurityManager.getSystemPrincipal(),
          csp: null,
        });
        if (!loadInBackground) {
          if (isBlankPageURL(homePage)) {
            gURLBar.select();
          } else {
            gBrowser.selectedBrowser.focus();
          }
        }
        aEvent?.preventDefault();
        break;
      }
      case "window":
        // OpenBrowserWindow will trigger the observer event, so no need to do so here.
        notifyObservers = false;
        OpenBrowserWindow();
        aEvent?.preventDefault();
        break;
    }

    if (notifyObservers) {
      // A notification for when a user has triggered their homepage. This is used
      // to display a doorhanger explaining that an extension has modified the
      // homepage, if necessary. Observers are only notified if the homepage
      // becomes the active page.
      Services.obs.notifyObservers(null, "browser-open-homepage-start");
    }
  },
};
