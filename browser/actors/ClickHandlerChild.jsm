/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["ClickHandlerChild", "MiddleMousePasteHandlerChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

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

class MiddleMousePasteHandlerChild extends JSWindowActorChild {
  handleEvent(clickEvent) {
    if (
      clickEvent.defaultPrevented ||
      clickEvent.button != 1 ||
      MiddleMousePasteHandlerChild.autoscrollEnabled
    ) {
      return;
    }
    this.manager
      .getActor("ClickHandler")
      .handleClickEvent(
        clickEvent,
        /* is from middle mouse paste handler */ true
      );
  }

  onProcessedClick(data) {
    this.sendAsyncMessage("MiddleClickPaste", data);
  }
}

XPCOMUtils.defineLazyPreferenceGetter(
  MiddleMousePasteHandlerChild,
  "autoscrollEnabled",
  "general.autoScroll",
  true
);

class ClickHandlerChild extends JSWindowActorChild {
  handleEvent(wrapperEvent) {
    this.handleClickEvent(wrapperEvent.sourceEvent);
  }

  handleClickEvent(event, isFromMiddleMousePasteHandler = false) {
    if (event.defaultPrevented || event.button == 2) {
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

    if (href && !isFromMiddleMousePasteHandler) {
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
    }

    // This might be middle mouse navigation, in which case pass this back:
    if (!href && event.button == 1 && isFromMiddleMousePasteHandler) {
      this.manager.getActor("MiddleMousePasteHandler").onProcessedClick(json);
    }
  }
}
