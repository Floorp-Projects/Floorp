/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetters(lazy, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
});

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
});

export var BrowserUIUtils = {
  /**
   * Check whether a page can be considered as 'empty', that its URI
   * reflects its origin, and that if it's loaded in a tab, that tab
   * could be considered 'empty' (e.g. like the result of opening
   * a 'blank' new tab).
   *
   * We have to do more than just check the URI, because especially
   * for things like about:blank, it is possible that the opener or
   * some other page has control over the contents of the page.
   *
   * @param {Browser} browser
   *        The browser whose page we're checking.
   * @param {nsIURI} [uri]
   *        The URI against which we're checking (the browser's currentURI
   *        if omitted).
   *
   * @return {boolean} false if the page was opened by or is controlled by
   *         arbitrary web content, unless that content corresponds with the URI.
   *         true if the page is blank and controlled by a principal matching
   *         that URI (or the system principal if the principal has no URI)
   */
  checkEmptyPageOrigin(browser, uri = browser.currentURI) {
    // If another page opened this page with e.g. window.open, this page might
    // be controlled by its opener.
    if (browser.hasContentOpener) {
      return false;
    }
    let contentPrincipal = browser.contentPrincipal;
    // Not all principals have URIs...
    // There are two special-cases involving about:blank. One is where
    // the user has manually loaded it and it got created with a null
    // principal. The other involves the case where we load
    // some other empty page in a browser and the current page is the
    // initial about:blank page (which has that as its principal, not
    // just URI in which case it could be web-based). Especially in
    // e10s, we need to tackle that case specifically to avoid race
    // conditions when updating the URL bar.
    //
    // Note that we check the documentURI here, since the currentURI on
    // the browser might have been set by SessionStore in order to
    // support switch-to-tab without having actually loaded the content
    // yet.
    let uriToCheck = browser.documentURI || uri;
    if (
      (uriToCheck.spec == "about:blank" && contentPrincipal.isNullPrincipal) ||
      contentPrincipal.spec == "about:blank"
    ) {
      return true;
    }
    if (contentPrincipal.isContentPrincipal) {
      return contentPrincipal.equalsURI(uri);
    }
    // ... so for those that don't have them, enforce that the page has the
    // system principal (this matches e.g. on about:newtab).
    return contentPrincipal.isSystemPrincipal;
  },

  /**
   * Generate a document fragment for a localized string that has DOM
   * node replacements. This avoids using getFormattedString followed
   * by assigning to innerHTML. Fluent can probably replace this when
   * it is in use everywhere.
   *
   * @param {Document} doc
   * @param {String}   msg
   *                   The string to put replacements in. Fetch from
   *                   a stringbundle using getString or GetStringFromName,
   *                   or even an inserted dtd string.
   * @param {Node|String} nodesOrStrings
   *                   The replacement items. Can be a mix of Nodes
   *                   and Strings. However, for correct behaviour, the
   *                   number of items provided needs to exactly match
   *                   the number of replacement strings in the l10n string.
   * @returns {DocumentFragment}
   *                   A document fragment. In the trivial case (no
   *                   replacements), this will simply be a fragment with 1
   *                   child, a text node containing the localized string.
   */
  getLocalizedFragment(doc, msg, ...nodesOrStrings) {
    // Ensure replacement points are indexed:
    for (let i = 1; i <= nodesOrStrings.length; i++) {
      if (!msg.includes("%" + i + "$S")) {
        msg = msg.replace(/%S/, "%" + i + "$S");
      }
    }
    let numberOfInsertionPoints = msg.match(/%\d+\$S/g).length;
    if (numberOfInsertionPoints != nodesOrStrings.length) {
      console.error(
        `Message has ${numberOfInsertionPoints} insertion points, ` +
          `but got ${nodesOrStrings.length} replacement parameters!`
      );
    }

    let fragment = doc.createDocumentFragment();
    let parts = [msg];
    let insertionPoint = 1;
    for (let replacement of nodesOrStrings) {
      let insertionString = "%" + insertionPoint++ + "$S";
      let partIndex = parts.findIndex(
        part => typeof part == "string" && part.includes(insertionString)
      );
      if (partIndex == -1) {
        fragment.appendChild(doc.createTextNode(msg));
        return fragment;
      }

      if (typeof replacement == "string") {
        parts[partIndex] = parts[partIndex].replace(
          insertionString,
          replacement
        );
      } else {
        let [firstBit, lastBit] = parts[partIndex].split(insertionString);
        parts.splice(partIndex, 1, firstBit, replacement, lastBit);
      }
    }

    // Put everything in a document fragment:
    for (let part of parts) {
      if (typeof part == "string") {
        if (part) {
          fragment.appendChild(doc.createTextNode(part));
        }
      } else {
        fragment.appendChild(part);
      }
    }
    return fragment;
  },

  removeSingleTrailingSlashFromURL(aURL) {
    // remove single trailing slash for http/https/ftp URLs
    return aURL.replace(/^((?:http|https|ftp):\/\/[^/]+)\/$/, "$1");
  },

  /**
   * Returns a URL which has been trimmed by removing 'http://' and any
   * trailing slash (in http/https/ftp urls).
   * Note that a trimmed url may not load the same page as the original url, so
   * before loading it, it must be passed through URIFixup, to check trimming
   * doesn't change its destination. We don't run the URIFixup check here,
   * because trimURL is in the page load path (see onLocationChange), so it
   * must be fast and simple.
   *
   * @param {string} aURL The URL to trim.
   * @returns {string} The trimmed string.
   */
  get trimURLProtocol() {
    return "http://";
  },
  trimURL(aURL) {
    let url = this.removeSingleTrailingSlashFromURL(aURL);
    // Remove "http://" prefix.
    return url.startsWith(this.trimURLProtocol)
      ? url.substring(this.trimURLProtocol.length)
      : url;
  },

  /**
   * Open a new browser window without being dependent on other windows.
   * @returns {ChromeWindow} A new browser window.
   */
  async openNewBrowserWindow() {
    var telemetryObj = {};
    TelemetryStopwatch.start("FX_NEW_WINDOW_MS", telemetryObj);

    let defaultArgs = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    defaultArgs.data = lazy.BrowserHandler.defaultArgs;

    let extraFeatures = "suppressanimation";

    let win = lazy.BrowserWindowTracker.openWindow({
      args: defaultArgs,
      features: extraFeatures,
    });

    win.addEventListener(
      "MozAfterPaint",
      () => {
        TelemetryStopwatch.finish("FX_NEW_WINDOW_MS", telemetryObj);
      },
      { once: true }
    );

    await this._topicObserved(
      "browser-delayed-startup-finished",
      subject => subject == win
    );

    win.focus();

    return win;
  },
  _topicObserved(observeTopic, checkFn) {
    return new Promise((resolve, reject) => {
      function observer(subject, topic, data) {
        try {
          if (checkFn && !checkFn(subject, data)) {
            return;
          }
          Services.obs.removeObserver(observer, topic);
          checkFn = null;
          resolve([subject, data]);
        } catch (ex) {
          Services.obs.removeObserver(observer, topic);
          checkFn = null;
          reject(ex);
        }
      }
      Services.obs.addObserver(observer, observeTopic);
    });
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  BrowserUIUtils,
  "quitShortcutDisabled",
  "browser.quitShortcut.disabled",
  false
);
