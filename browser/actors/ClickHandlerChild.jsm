/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["ClickHandlerChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "WebNavigationFrames",
  "resource://gre/modules/WebNavigationFrames.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);

class ClickHandlerChild extends JSWindowActorChild {
  handleEvent(event) {
    if (
      event.defaultPrevented ||
      event.button == 2 ||
      (event.type == "click" && event.button == 1)
    ) {
      return;
    }
    // Don't do anything on editable things, we shouldn't open links in
    // contenteditables, and editor needs to possibly handle middlemouse paste
    let composedTarget = event.composedTarget;
    if (
      composedTarget.isContentEditable ||
      (composedTarget.ownerDocument &&
        composedTarget.ownerDocument.designMode == "on") ||
      ChromeUtils.getClassName(composedTarget) == "HTMLInputElement" ||
      ChromeUtils.getClassName(composedTarget) == "HTMLTextAreaElement"
    ) {
      return;
    }

    let originalTarget = event.originalTarget;
    let ownerDoc = originalTarget.ownerDocument;
    if (!ownerDoc) {
      return;
    }

    // Handle click events from about pages
    if (event.button == 0) {
      if (ownerDoc.documentURI.startsWith("about:blocked")) {
        return;
      }
    }

    // For untrusted events, require a valid transient user gesture activation.
    if (!event.isTrusted && !ownerDoc.hasValidTransientUserGestureActivation) {
      return;
    }

    let [href, node, principal] = BrowserUtils.hrefAndLinkNodeForClickEvent(
      event
    );

    let csp = ownerDoc.csp;
    if (csp) {
      csp = E10SUtils.serializeCSP(csp);
    }

    let referrerInfo = Cc["@mozilla.org/referrer-info;1"].createInstance(
      Ci.nsIReferrerInfo
    );
    if (node) {
      referrerInfo.initWithElement(node);
    } else {
      referrerInfo.initWithDocument(ownerDoc);
    }
    referrerInfo = E10SUtils.serializeReferrerInfo(referrerInfo);
    let frameID = WebNavigationFrames.getFrameId(ownerDoc.defaultView);

    let json = {
      button: event.button,
      shiftKey: event.shiftKey,
      ctrlKey: event.ctrlKey,
      metaKey: event.metaKey,
      altKey: event.altKey,
      href: null,
      title: null,
      frameID,
      triggeringPrincipal: principal,
      csp,
      referrerInfo,
      originAttributes: principal ? principal.originAttributes : {},
      isContentWindowPrivate: PrivateBrowsingUtils.isContentWindowPrivate(
        ownerDoc.defaultView
      ),
    };

    if (href) {
      try {
        Services.scriptSecurityManager.checkLoadURIStrWithPrincipal(
          principal,
          href
        );
      } catch (e) {
        return;
      }

      if (
        !event.isTrusted &&
        BrowserUtils.whereToOpenLink(event) != "current"
      ) {
        // If we'll open the link, we want to consume the user gesture
        // activation to ensure that we don't allow multiple links to open
        // from one user gesture.
        // Avoid doing so for links opened in the current tab, which get
        // handled later, by gecko, as otherwise its popup blocker will stop
        // the link from opening.
        // We will do the same check (whereToOpenLink) again in the parent and
        // avoid handling the click for such links... but we still need the
        // click information in the parent because otherwise places link
        // tracking breaks. (bug 1742894 tracks improving this.)
        ownerDoc.consumeTransientUserGestureActivation();
        // We don't care about the return value because we already checked that
        // hasValidTransientUserGestureActivation was true earlier in this
        // function.
      }

      json.href = href;
      if (node) {
        json.title = node.getAttribute("title");
      }

      json.originPrincipal = ownerDoc.nodePrincipal;
      json.originStoragePrincipal = ownerDoc.effectiveStoragePrincipal;
      json.triggeringPrincipal = ownerDoc.nodePrincipal;

      // If a link element is clicked with middle button, user wants to open
      // the link somewhere rather than pasting clipboard content.  Therefore,
      // when it's clicked with middle button, we should prevent multiple
      // actions here to avoid leaking clipboard content unexpectedly.
      // Note that whether the link will work actually or not does not matter
      // because in this case, user does not intent to paste clipboard content.
      if (event.button === 1) {
        event.preventMultipleActions();
      }

      this.sendAsyncMessage("Content:Click", json);
      return;
    }

    // This might be middle mouse navigation.
    if (event.button == 1) {
      this.sendAsyncMessage("Content:Click", json);
    }
  }
}
