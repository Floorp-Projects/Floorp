/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ClickHandlerParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PlacesUIUtils",
  "resource:///modules/PlacesUIUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm"
);

let gContentClickListeners = new Set();

class ClickHandlerParent extends JSWindowActorParent {
  static addContentClickListener(listener) {
    gContentClickListeners.add(listener);
  }

  static removeContentClickListener(listener) {
    gContentClickListeners.delete(listener);
  }

  receiveMessage(message) {
    switch (message.name) {
      case "Content:Click":
        this.contentAreaClick(message.data);
        this.notifyClickListeners(message.data);
        break;
    }
  }

  /**
   * Handles clicks in the content area.
   *
   * @param data {Object} object that looks like an Event
   * @param browser {Element<browser>}
   */
  contentAreaClick(data) {
    // This is heavily based on contentAreaClick from browser.js (Bug 903016)
    // The data is set up in a way to look like an Event.
    let browser = this.manager.browsingContext.top.embedderElement;
    let window = browser.ownerGlobal;

    if (!data.href) {
      // Might be middle mouse navigation.
      if (
        Services.prefs.getBoolPref("middlemouse.contentLoadURL") &&
        !Services.prefs.getBoolPref("general.autoScroll")
      ) {
        window.middleMousePaste(data);
      }
      return;
    }

    // If the browser is not in a place where we can open links, bail out.
    // This can happen in osx sheets, dialogs, etc. that are not browser
    // windows.  Specifically the payments UI is in an osx sheet.
    if (window.openLinkIn === undefined) {
      return;
    }

    // Mark the page as a user followed link.  This is done so that history can
    // distinguish automatic embed visits from user activated ones.  For example
    // pages loaded in frames are embed visits and lost with the session, while
    // visits across frames should be preserved.
    try {
      if (!PrivateBrowsingUtils.isWindowPrivate(window)) {
        PlacesUIUtils.markPageAsFollowedLink(data.href);
      }
    } catch (ex) {
      /* Skip invalid URIs. */
    }

    // This part is based on handleLinkClick.
    var where = window.whereToOpenLink(data);
    if (where == "current") {
      return;
    }

    // Todo(903022): code for where == save

    let params = {
      charset: browser.characterSet,
      referrerInfo: E10SUtils.deserializeReferrerInfo(data.referrerInfo),
      isContentWindowPrivate: data.isContentWindowPrivate,
      originPrincipal: data.originPrincipal,
      originStoragePrincipal: data.originStoragePrincipal,
      triggeringPrincipal: data.triggeringPrincipal,
      csp: data.csp ? E10SUtils.deserializeCSP(data.csp) : null,
      frameID: data.frameID,
    };

    // The new tab/window must use the same userContextId.
    if (data.originAttributes.userContextId) {
      params.userContextId = data.originAttributes.userContextId;
    }

    params.allowInheritPrincipal = true;

    window.openLinkIn(data.href, where, params);
  }

  notifyClickListeners(data) {
    for (let listener of gContentClickListeners) {
      try {
        let browser = this.browsingContext.top.embedderElement;

        listener.onContentClick(browser, data);
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
  }
}
