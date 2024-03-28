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
};
