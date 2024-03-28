/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

"use strict";

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
};
